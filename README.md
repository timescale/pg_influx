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
- [Contribution Guidelines](CONTRIBUTING.md)

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

### Building from source

After the dependencies are installed, you can download the repository
from GitHub.

```bash
git clone git@github.com:mkindahl/pg_influx.git
cd pg_influx
```

To build and install the extension:

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

### Adding PostgreSQL versions

If a new version of PostgreSQL is supported, it is necessary to
rebuild the `debian/control` file from the `debian/control.in` file.

```bash
pg_buildext updatecontrol
```

## Running Regression Tests

To run the regression tests

```
make installcheck
```
