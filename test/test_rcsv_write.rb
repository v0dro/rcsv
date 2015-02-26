require 'test/unit'
require 'rcsv'
require 'date'

class RcsvWriteTest < Test::Unit::TestCase
  def setup
    hashformat = if RUBY_VERSION >= '1.9'
      {
        :name => 'Hashformat',
        :formatter => :printf,
        :format => '%{currency_symbol}%<field>2.2f%{nilly}',
        :printf_options => {:currency_symbol => '$', :nilly => nil}
      }
    else # Ruby before 1.9 didn't support Hash formatting for sprintf()
      {
        :name => 'Hashformat',
        :formatter => :printf,
        :format => '$%2.2f'
      }
    end

    @options = {
      :header => true,
      :columns => [
        {
          :name => 'ID'
        },
        {
          :name => 'Date',
          :formatter => :strftime,
          :format => '%Y-%m-%d'
        },
        {
          :name => 'Money',
          :formatter => :printf,
          :format => '$%2.2f'
        },
        {
          :name => 'Banana IDDQD'
        },
        hashformat,
        {
          :name => nil,
          :formatter => :boolean
        }
      ]
    }

    @data = [
      [1, Date.parse('2012-11-11'), 100.234, true, 1, true],
      ['elephant', Date.parse('1970-01-02'), -0.1, :nyancat, 123.8891, 0],
      [3, Date.parse('2012-12-12'), 0, 'sepulka', -122, 'zoop'],
      [nil, nil, nil, nil, nil, nil]
    ]

    @writer = Rcsv.new(@options)
  end

  def test_rcsv_generate_header
    assert_equal(
      "ID,Date,Money,Banana IDDQD,Hashformat,\r\n", @writer.generate_header
    )
  end

  def test_rscv_generate_row
    assert_equal("1,2012-11-11,$100.23,true,$1.00,true\r\n", @writer.generate_row(@data.first))
  end

  def test_rcsv_write
    io = StringIO.new

    @writer.write(io) do
      @data.shift
    end

    io.rewind

    assert_equal(
      "ID,Date,Money,Banana IDDQD,Hashformat,\r\n1,2012-11-11,$100.23,true,$1.00,true\r\nelephant,1970-01-02,$-0.10,nyancat,$123.89,false\r\n3,2012-12-12,$0.00,sepulka,$-122.00,true\r\n,,,,,\r\n", io.read
    )
  end

  def test_rcsv_write_no_headers
    io = StringIO.new
    @writer.write_options[:header] = false

    @writer.write(io) do
      @data.shift
    end

    io.rewind

    assert_equal(
      "1,2012-11-11,$100.23,true,$1.00,true\r\nelephant,1970-01-02,$-0.10,nyancat,$123.89,false\r\n3,2012-12-12,$0.00,sepulka,$-122.00,true\r\n,,,,,\r\n", io.read
    )
  end
end
