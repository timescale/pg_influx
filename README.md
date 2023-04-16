# InfluxDB Line Protocol Listener for PostgreSQL

InfluxDB line protocol listener that can listen on UDP ports and
insert into PostgreSQL database tables. 

Lines will be received over UDP in InfluxDB line protocol format and
write them into tables in the database. This means that there is a
risk that lines can be lost, but it is very fast and does not block.

This is a very basic example of an extension that listens for data on
a socket and writes it to the database.

It is an ongoing experimental work for [educational purposes][1] and
as such there are no guarantees regarding feasability for any specific
purpose, including its intended use.

[1]: https://dbmsdrops.kindahl.net/

## Documentation

- [Documentation](docs/index.md)

## Building and Installing

To install the dependencies it is easiest to install the [PGDG
version](https://wiki.postgresql.org/wiki/Apt) of the packages. Using
the instructions from [PostgreSQL Apt
Repository](https://www.postgresql.org/download/linux/ubuntu/) would
look like this:

```bash
sudo sh -c 'echo "deb http://apt.postgresql.org/pub/repos/apt $(lsb_release -cs)-pgdg main" > /etc/apt/sources.list.d/pgdg.list'
wget --quiet -O - https://www.postgresql.org/media/keys/ACCC4CF8.asc | sudo apt-key add -
sudo apt-get update
sudo apt-get -y install postgresql postgresql-server-dev-13
```

After the dependencies are installed, you can download the repository
from GitHub.

```bash
git clone git@github.com:mkindahl/pg_influx.git
cd pg_influx
```

### Building from source

After the dependencies are installed, To build and install the
extension:

```bash
make
sudo make install
```

### Building Debian Packages

The `debian` directory contains the necessary files to build a debian
package. To build the Debian packages you can use `debmake` together
with `debuild`:

```bash
debmake -t -i debuild
```

You will find the resulting packages in the parent directory of the
source directory.

If you want to build unsigned packages, you can do that using

```bash
debmake -t -i "debuild -i -uc -us"
```

## Running Regression Tests

To run the regression tests

```
make installcheck
```

## InfluxDB Line Protocol

The listener uses the [Influx line protocol][1], which is a simple
protocol for sending metric data that is supported by [Telegraf][2].

[1]: https://docs.influxdata.com/influxdb/cloud/reference/syntax/line-protocol/
[2]: https://www.influxdata.com/time-series-platform/telegraf/

Each line of the line protocol consists of lines of the format:

```
measurementName,tagKey=tagValue fieldKey="fieldValue" 1465839830100400200
```

There is a single measurement name (also called "metric"), zero or
more tag keys with values, and at least one field with a field
value. The timestamp is optional, and is given in nanoseconds since
the epoch.  When parsing the line, quotes are removed from any quoted
field value before being used as a literal string.

The grammar is very simple, but we use an even more simplified
version. We allow `\` to escape any character in values and keys and
the actual interpretation of the strings will be done at a later
stage, so we do not bother about distinguishing integers from
floating-point numbers, nor bare strings from quoted strings (but we
remove the quotes).

```ebnf
Line = Ident, {",", Item}, " ", Item, {",", Item};
Item = Ident, "=", Value;
Ident = LETTER, {LETTER | DIGIT | "_" | "-"};
Value = QString | BString;
BString = LETTER, {"\", ANY | VALUE_CHAR};
QString = '"', {"\", ANY | STRING_CHAR}, '"';
VALUE_CHAR = ? any character except space, comma, or backslash ?;
STRING_CHAR = ? any character except quote or backslash ?;
LETTER = ? any letter ?;
DIGIT = ? any digit ?;
```

## InfluxDB Ports

| Port | Protocol | Description                                           |
|:-----|:---------|:------------------------------------------------------|
| 8089 | UDP      | The default port that runs the UDP service.           |
| 8086 | HTTP/TCP | The default port that runs the InfluxDB HTTP service. |
