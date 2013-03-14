require 'test/unit'
require 'rcsv'
require 'date'

class RcsvWriteTest < Test::Unit::TestCase
  def setup
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
        {
          :name => nil,
          :formatter => :boolean
        }
      ]
    }

    @data = [
      [1, Date.parse('2012-11-11'), 100.234, true, nil],
      [nil, Date.parse('1970-01-02'), -0.1, :nyancat, 0],
      [3, Date.parse('2012-12-12'), 0, 'sepulka', 'zoop']
    ]

    @writer = Rcsv.new(@options)
  end

  def test_rcsv_generate_header
    assert_equal(
      "ID,Date,Money,Banana IDDQD,\r\n", @writer.generate_header
    )
  end

  def test_rscv_generate_row
    assert_equal("1,2012-11-11,$100.23,true,false\r\n", @writer.generate_row(@data.first))
  end

  def test_rcsv_write
    io = StringIO.new

    @writer.write(io) do
      @data.shift
    end

    io.rewind

    assert_equal(
      "ID,Date,Money,Banana IDDQD,\r\n1,2012-11-11,$100.23,true,false\r\n,1970-01-02,$-0.10,nyancat,false\r\n3,2012-12-12,$0.00,sepulka,true\r\n", io.read
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
      "1,2012-11-11,$100.23,true,false\r\n,1970-01-02,$-0.10,nyancat,false\r\n3,2012-12-12,$0.00,sepulka,true\r\n", io.read
    )
  end
end
