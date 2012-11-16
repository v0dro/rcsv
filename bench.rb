require 'benchmark'

require 'csv'
#require './lib/lib_csv'
require 'rcsv'

TIMES = 10

# That CSV file contains "broken" headers that FaterCSV doesn't like.
# Remove all quotes from the header in order to fix this benchmark.
# But even better would be to test against much bigger CSV file.
data = File.read('./test/test_rcsv.csv')

Benchmark.bmbm do |b|
  b.report("FasterCSV") {
    TIMES.times {
      str = CSV.parse(data)
    }
  }

#  b.report("lib_csv") {
#    TIMES.times {
#      str = LibCsv.parse(data)
#    }
#  }

  b.report("rcsv") {
    TIMES.times {
      str = Rcsv.parse(data)
    }
  }
end
