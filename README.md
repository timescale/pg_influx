# InfluxDB Line Protocol Listener for PostgreSQL

A simple InfluxDB line protocol listener.

This is a very basic example of an extension that listens for data on
a socket and writes it to the database.

## Building and Installing

To build and install the extension:

```
git clone git@github.com:mkindahl/pg_influx.git
cd pg_influx
make
sudo make install
```

## Running Regression Tests

To run the regression tests

```
make installcheck
```

