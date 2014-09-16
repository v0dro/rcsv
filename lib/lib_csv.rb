require 'ffi'

class LibCsv
  extend FFI::Library
  ffi_lib 'libcsv'

  class CsvParser < FFI::Struct
    layout :pstate, :int,
           :qouted, :int,
           :spaces, :size_t,
           :entry_buf, :string,
           :entry_pos, :size_t,
           :entry_size, :size_t,
           :status, :int,
           :options, :uchar,
           :quote_char, :uchar,
           :delim_char, :uchar,
           :is_space, :pointer,
           :is_term, :pointer,
           :blk_size, :size_t,
           :malloc_func, :pointer,
           :realloc_func, :pointer,
           :free_func, :pointer
  end

  callback :end_of_field_callback, [:pointer, :size_t, :pointer], :void
  callback :end_of_record_callback, [:int, :pointer], :void

  attach_function :csv_init, [:pointer, :uchar], :int
  attach_function :csv_parse, [:pointer, :pointer, :size_t, :end_of_field_callback, :end_of_record_callback, :pointer], :size_t
  attach_function :csv_fini, [:pointer, :end_of_field_callback, :end_of_record_callback, :pointer], :int
  attach_function :csv_free, [:pointer], :void

  attach_function :csv_set_delim, [:pointer, :uchar], :void
  attach_function :csv_get_delim, [:pointer], :uchar

  attach_function :csv_set_quote, [:pointer, :uchar], :void
  attach_function :csv_get_quote, [:pointer], :uchar

  attach_function :csv_error, [:pointer], :int
  attach_function :csv_strerror, [:int], :string

  def self.parse(string, options = {})
    pointer = FFI::MemoryPointer.new :char, CsvParser.size, false
    parser = CsvParser.new pointer
    result = csv_init(parser, 0)

    if options[:col_sep]
      csv_set_delim(parser, options[:col_sep].ord)
    end

    if options[:quote_char]
      csv_set_quote(parser, options[:quote_char].ord)
    end

    fail "Couldn't initialize libcsv" if result == -1

    result = [[]]

    end_of_field_callback = Proc.new { |p_field, field_size, p_data|
      str = p_field.read_pointer.null? ? nil : p_field.read_string(field_size)
      result.last << str
    }

    end_of_record_callback = Proc.new { |last_char, p_data|
      result << [] unless last_char == -1
    }

    original_length = string.bytesize
    length = nil

    length = csv_parse(parser, string, original_length, end_of_field_callback, end_of_record_callback, nil)

    unless length == original_length
      case error = csv_error(parser)
        when CSV_EPARSE
          fail "Error when parsing malformed data"
        when CSV_ENOMEM
          fail "No memory"
        when CSV_ETOOBIG
          fail "Too large field data"
        when CSV_EINVALID
          fail csv_strerror(error)
        else
          fail "Failed due to unknown reason"
      end
    end

    csv_fini(parser, end_of_field_callback, end_of_record_callback, nil)
    csv_free(parser)
    result.pop if result.last == []

    return result
  end
end
