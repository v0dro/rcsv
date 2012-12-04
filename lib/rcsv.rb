require "rcsv/rcsv"
require "rcsv/version"

require "stringio"

class Rcsv
  def self.parse(csv_data, options = {}, &block)
    #options = {
      #:column_separator => "\t",
      #:only_listed_columns => true,
      #:header => :use, # :skip, :none
      #:offset_rows => 10,
      #:columns => {
        #'a' => { # can be 0, 1, 2, ... -- column position
          #:alias => :a, # only for hashes
          #:type => :int,
          #:default => 100,
          #:match => '10'
        #},
        #...
      #}
    #}

    options[:header] ||= :use
    raw_options = {}

    raw_options[:col_sep] = options[:column_separator] && options[:column_separator][0] || ','
    raw_options[:offset_rows] = options[:offset_rows] || 0
    raw_options[:nostrict] = options[:nostrict]
    raw_options[:buffer_size] = options[:buffer_size] || 1024 * 1024 # 1 MiB

    if csv_data.is_a?(String)
      csv_data = StringIO.new(csv_data)
    elsif !(csv_data.respond_to?(:lines) && csv_data.respond_to?(:read))
      inspected_csv_data = csv_data.inspect
      raise ParseError.new("Supplied CSV object #{inspected_csv_data[0..127]}#{inspected_csv_data.size > 128 ? '...' : ''} is neither String nor looks like IO object.")
    end

    initial_position = csv_data.pos

    case options[:header]
    when :use
      header = self.raw_parse(StringIO.new(csv_data.lines.first), raw_options).first
      raw_options[:offset_rows] += 1
    when :skip
      header = (0..(csv_data.lines.first.split(raw_options[:col_sep]).count)).to_a
      raw_options[:offset_rows] += 1
    when :none
      header = (0..(csv_data.lines.first.split(raw_options[:col_sep]).count)).to_a
    end

    raw_options[:row_as_hash] = options[:row_as_hash] # Setting after header parsing

    if options[:columns]
      only_rows = []
      row_defaults = []
      column_names = []
      row_conversions = ''

      header.each do |column_header|
        column_options = options[:columns][column_header]
        if column_options
          if (options[:row_as_hash])
            column_names << column_options[:alias] || column_header
          end
          row_defaults << column_options[:default] || nil
          only_rows << column_options[:match] || nil
          row_conversions << case column_options[:type]
          when :int
            'i'
          when :float
            'f'
          when :string
            's'
          when :bool
            'b'
          when nil
            's' # strings by default
          else
            fail "Unknown column type"
          end
        elsif options[:only_listed_columns]
          column_names << nil
          row_defaults << nil
          only_rows << nil
          row_conversions << ' '
        else
          column_names << column_header
          row_defaults << nil
          only_rows << nil
          row_conversions << 's'
        end
      end

      raw_options[:column_names] = column_names if options[:row_as_hash]
      raw_options[:only_rows] = only_rows
      raw_options[:row_defaults] = row_defaults
      raw_options[:row_conversions] = row_conversions
    end

    csv_data.pos = initial_position
    return self.raw_parse(csv_data, raw_options, &block)
  end
end
