#ifndef RATELIMITER_H
#define RATELIMITER_H

#include <QElapsedTimer>
#include <QtGlobal>

/*!
 * Token-bucket rate limiter for one TCP connection.
 *
 * The bucket starts full (burst capacity) and refills at a steady rate,
 * so short bursts of legitimate requests pass while sustained flooding
 * is rejected.  Time is injected by the caller (milliseconds), which
 * keeps the class deterministic and directly unit-testable.
 *
 * Used by ConnectionHandler to shed abusive clients before they reach
 * the SQLite layer; the handler sends a rate_limit_error and closes the
 * socket when allow() returns false.
 */
class RateLimiter
{
public:
    explicit RateLimiter(int burst = 60, double refillPerSecond = 25.0)
        : m_tokens(burst)
    {
        Q_ASSERT(burst > 0);
        Q_ASSERT(refillPerSecond > 0.0);
        m_burst = burst;
        m_refillPerMs = refillPerSecond / 1000.0;
        m_lastMs = 0;
        m_started = false;
    }

    //! Returns true when one request is admitted at time \p nowMs.
    bool allow(qint64 nowMs)
    {
        if (!m_started) {
            m_started = true;
            m_lastMs = nowMs;
        } else if (nowMs > m_lastMs) {
            // Refill the bucket, capped at its burst size.
            m_tokens = qMin(double(m_burst),
                            m_tokens + double(nowMs - m_lastMs) * m_refillPerMs);
            m_lastMs = nowMs;
        }
        if (m_tokens >= 1.0) {
            m_tokens -= 1.0;
            return true;
        }
        return false;
    }

    //! Tokens currently available (fractional), mostly for tests/logging.
    double tokens() const { return m_tokens; }

private:
    int m_burst = 0;
    double m_refillPerMs = 0.0;
    double m_tokens = 0.0;
    qint64 m_lastMs = 0;
    bool m_started = false;
};

#endif // RATELIMITER_H
