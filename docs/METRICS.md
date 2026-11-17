# Prometheus metrics (v2.7)

The server can expose its operational counters in the
[Prometheus text exposition format](https://prometheus.io/docs/instrumenting/exposition_formats/)
(version 0.0.4) on a dedicated port. Any scraper, dashboard or alert
rule speaks it natively — no admin login, no IUT-Food client needed.

```
iut-food-server --port 1234 --metrics-port 9101
curl http://127.0.0.1:9101/metrics
```

The endpoint is **plain HTTP on purpose**: scraping belongs on an
internal network (or behind a sidecar/reverse proxy that terminates
TLS). Do not forward the metrics port to the internet — it reveals
nothing about users or orders, but it does tell an attacker how busy
(and which version) the server is.

## Exported metrics

| Metric | Type | Meaning |
|---|---|---|
| `iut_food_build_info{version="…"}` | gauge | Release metadata, always `1`. The label names the running version — the Prometheus way to answer "what is deployed?" |
| `iut_food_uptime_seconds` | gauge | Seconds since the server process started. |
| `iut_food_sessions_active` | gauge | Session tokens currently valid (the same number the admin health tab shows). |
| `iut_food_sessions_total` | counter | Sessions opened since startup. |
| `iut_food_connections_accepted_total` | counter | TCP connections accepted on the main port. |
| `iut_food_tls_connections_total` | counter | Connections that completed a TLS handshake. |
| `iut_food_requests_served_total` | counter | Protocol commands dispatched (all of them, including failed ones). |
| `iut_food_requests_rejected_total` | counter | Rate-limit / frame-cap rejections. |
| `iut_food_auth_failures_total` | counter | Missing, expired or low-role tokens. |

Every family ships `# HELP` and `# TYPE` lines; every value is a plain
integer. The metrics live in `src/server/metricsserver.h` (header-only,
one-shot HTTP semantics, 404 for unknown paths, 405 for non-GET
methods) and are pinned by the `metrics_tests` suite.

`get_server_stats` (admin protocol command) and the metrics endpoint
read the same counters — the admin tab and a Grafana panel can never
disagree about reality.

## Scrape configuration

```yaml
# prometheus.yml
scrape_configs:
  - job_name: iut-food
    scrape_interval: 15s
    static_configs:
      - targets: ["iut-food-server.internal:9101"]
```

With `docker compose up` the compose file already publishes the port:

```yaml
services:
  server:
    command: ["--port", "1234", ..., "--metrics-port", "9101"]
    ports:
      - "1234:1234"
      - "9101:9101"
```

## Useful PromQL

```promql
# Request rate over the last 5 minutes
rate(iut_food_requests_served_total[5m])

# Auth failure ratio (a spike usually means a credential attack)
sum(rate(iut_food_auth_failures_total[5m]))
  / clamp_min(sum(rate(iut_food_requests_served_total[5m])), 1e-9)

# Rate-limited clients (abuse detection)
increase(iut_food_requests_rejected_total[10m])

# Which version is running (alert when it lags the latest release)
max by (version) (iut_food_build_info)
```
