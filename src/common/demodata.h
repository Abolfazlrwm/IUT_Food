#ifndef DEMODATA_H
#define DEMODATA_H

class DataBaseHandler;

/*!
 * Populates a fresh database with a small, realistic dataset so the app
 * is presentable out of the box.  Called automatically on first launch
 * (when the users table is empty) by both the client and the server.
 */
void seedDemoData(DataBaseHandler &db);

#endif // DEMODATA_H
