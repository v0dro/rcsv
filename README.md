# Rcsv

[![Build Status](https://travis-ci.org/fiksu/rcsv.png)](https://travis-ci.org/fiksu/rcsv)

Rcsv is a fast CSV parsing library for MRI Ruby. Tested on REE 1.8.7 and Ruby 1.9.3.

Contrary to many other gems that implement their own parsers, Rcsv uses libcsv 3.0.2 (http://sourceforge.net/projects/libcsv/). As long as libcsv's API is stable, getting Rcsv to use newer libcsv version is as simple as updating two files (csv.h and libcsv.c).

## Benchmarks
                   user     system      total        real
    FasterCSV   0.580000   0.000000   0.580000 (  0.618837)
    rcsv        0.060000   0.000000   0.060000 (  0.062248)

## License

Rcsv itself is distributed under BSD-derived license (see LICENSE) except for included csv.h and libcsv.c source files that are distributed under LGPL v2.1 (see COPYING.LESSER). Libcsv sources were not modified in any manner.

## Installation

Add this line to your application's Gemfile:

    gem 'rcsv'

And then execute:

    $ bundle

Or install it yourself as:

    $ gem install rcsv


## Building the latest source

First, check out the master branch. Then cd there and run:

    $ bundle                  # Installs development dependencies
    $ bundle exec rake        # Runs tests
    $ gem build rcsv.gemspec  # Builds the gem

## Usage

Currently, Rcsv only supports CSV parsing. CSV write support is planned.

Quickstart:

    parsed = Rcsv.parse(csv_data)


Rcsv class exposes a class method *parse* that accepts a CSV string as its first parameter and options hash as its second parameter.


Options supported:

### :column_separator

A single-character string that is used as a separator. Default is ",".

### :nostrict

A boolean flag. When enabled, allows to parse oddly quoted CSV data without exceptions being raised. Disabled by default.

Anything that does not conform to http://www.ietf.org/rfc/rfc4180.txt should better be parsed with this option enabled.

### :offset_rows

A positive integer that specifies how many rows should be skipped, counting from the beginning. Default is 0.

### :columns
A hash that contains per-column parsing instructions. By default, every CSV cell is parsed as a raw string without conversions. Empty strings are parsed as nils.

If CSV has a header, :columns keys can be strings that are equal to column names in the header. If there is no header, keys should represent integer column positions.

:columns values are in turn hashes that provide parsing options:

* :alias - Object of any type (though usually a Symbol) that is used to as a key that represents column name when :row_as_hash is set.
* :type - A Ruby Symbol that specifies Ruby data type that CSV cell value should be converted into. Supported types: :int, :float, :string, :bool. :string is the default.
* :default - Object of any type (though usually of the same type that is specified by :type option). If CSV doesn't have any value for a cell, this default value is used.
* :match - A string. If set, makes Rcsv skip all the rows where any column doesn't match its :match value. Useful for filtering data.


### :header
A Ruby symbol that specifies how CSV header should be processed. Accepted values:

* :use (default) - If :columns is set, instructs Rcsv to parse the first CSV line and use column names from there as :columns keys. Ignores the header when :columns is not set.

* :skip - Skips the header, treats :columns keys as column positions.

* :none - Tells Rcsv that CSV header is not present. :columns keys are treated as column positions.

### :row_as_hash
A boolean flag. Disabled by default.
When enabled, *parse* return value is represented as array of hashes. If :header is set to :use, keys for hashes are either string column names from CSV header or their aliases. Otherwise, column indexes are used.
When :row_as_hash is disabled, return value is represented as array of arrays.

### :only_listed_columns
A boolean flag. If enabled, only parses columns that are listed in :columns. Disabled by default.


## Examples

This example parses a 3-column CSV file and only returns parsed rows where "Age" values are set to "35".

    Rcsv.parse some_csv, :row_as_hash => true,
                         :columns => {
      'First Name' => { :alias => :first_name, :default => "Unknown" },
      'Last Name' => { :alias => :last_name, :default => "Unknown"},
      'Age' => { :alias => :age, :type => :int, :match => "35"}
    }

The result would look like this:

    [
      { :first_name => "Mary", :last_name => "Jane", :age => 35 },
      { :first_name => "Unknown", :last_name => "Alien", :age => 35}
    ]

Another example, for a miserable headerless Tab-separated CSV:

    Rcsv.parse some_csv, :column_separator => "\t",
                         :header => :none,
                         :columns => {
      1 => { :type => :float, :default => 0 }
    }

The result would look like this:

    [
      [ "Very hot", 3.7, "Mercury" ],
      [ "Very hot and cloudy", 8.87, "Venus" ],
      [ "Just about ok", 9.78, "Earth"],
      [ nil, 0, "Vacuum" ]
    ]


## To do

* More specs for boolean values
* Specs for Ruby parse
* Add custom Ruby callbacks (if block is passed)
* Add CSV write support


## Contributing

1. Fork it
2. Create your feature branch (`git checkout -b my-new-feature`)
3. Commit your changes (`git commit -am 'Added some feature'`)
4. Push to the branch (`git push origin my-new-feature`)
5. Create new Pull Request
