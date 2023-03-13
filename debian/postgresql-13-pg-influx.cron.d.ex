#
# Regular cron jobs for the postgresql-13-pg-influx package.
#
0 4	* * *	root	[ -x /usr/bin/postgresql-13-pg-influx_maintenance ] && /usr/bin/postgresql-13-pg-influx_maintenance
