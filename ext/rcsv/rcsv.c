#include <stdbool.h>
#include <ruby.h>

#include "csv.h"

static VALUE rcsv_parse_error; /* class Rcsv::ParseError << StandardError; end */

/* It is useful to know exact row/column positions and field contents where parse-time exception was raised */
#define RAISE_WITH_LOCATION(row, column, contents, fmt, ...) \
  rb_raise(rcsv_parse_error, "[%d:%d '%s'] " fmt, (int)(row), (int)(column), (char *)(contents), ##__VA_ARGS__);

struct rcsv_metadata {
  /* Derived from user-specified options */
  bool row_as_hash;           /* Used to return array of hashes rather than array of arrays */
  size_t offset_rows;         /* Number of rows to skip before parsing */

  char * row_conversions;     /* A pointer to string/array of row conversions char specifiers */
  char ** only_rows;          /* A pointer to array of strings for only_rows filter */
  VALUE * row_defaults;       /* A pointer to array of row defaults */
  VALUE * column_names;       /* A pointer to array of column names to be used with hashes */

  /* Pointer options lengths */
  size_t num_row_conversions; /* Number of converter types in row_conversions array */
  size_t num_only_rows;       /* Number of items in only_rows filter */
  size_t num_row_defaults;    /* Number of default values in row_defaults array */
  size_t num_columns;         /* Number of columns detected from column_names.size */

  /* Internal state */
  bool skip_current_row;      /* Used by only_rows filter to skip parsing of the row remainder */
  size_t current_col;         /* Current column's index */
  size_t current_row;         /* Current row's index */

  VALUE last_entry;           /* A pointer to the last entry that's going to be appended to result */
  VALUE * result;             /* A pointer to the parsed data */
};

/* Internal callbacks */

/* This procedure is called for every parsed field */
void end_of_field_callback(void * field, size_t field_size, void * data) {
  const char * field_str = (char *)field;
  struct rcsv_metadata * meta = (struct rcsv_metadata *) data;
  char row_conversion = 0;
  VALUE parsed_field;

  /* No need to parse anything until the end of the line if skip_current_row is set */
  if (meta->skip_current_row) {
    return;
  }

  /* Skip the row if its position is less than specifed offset */
  if (meta->current_row < meta->offset_rows) {
    meta->skip_current_row = true;
    return;
  }

  /* Filter by string row values listed in meta->only_rows */
  if ((meta->only_rows != NULL) &&
      (meta->current_col < meta->num_only_rows) &&
      (meta->only_rows[meta->current_col] != NULL) &&
      (strcmp(meta->only_rows[meta->current_col], field_str))) {
    meta->skip_current_row = true;
    return;
  }

  /* Get row conversion char specifier */
  if (meta->current_col < meta->num_row_conversions) {
    row_conversion = (char)meta->row_conversions[meta->current_col];
  }

  /* Convert the field from string into Ruby type specified by row_conversion */
  if (row_conversion != ' ') { /* spacebar skips the column */
    if (field_size == 0) {
      /* Assigning appropriate default value if applicable. */
      if (meta->current_col < meta->num_row_defaults) {
        parsed_field = meta->row_defaults[meta->current_col];
      } else { /* By default, default is nil */
        parsed_field = Qnil;
      }
    } else {
      if (meta->current_col < meta->num_row_conversions) {
        switch (row_conversion){
          case 's': /* String */
            parsed_field = rb_str_new(field_str, field_size); 
            break;
          case 'i': /* Integer */
            parsed_field = INT2NUM(atol(field_str));
            break;
          case 'f': /* Float */
            parsed_field = rb_float_new(atof(field_str));
            break;
          case 'b': /* TrueClass/FalseClass */
            switch (field_str[0]) {
              case 't':
              case 'T':
              case '1':
                parsed_field = Qtrue;
                break;
              case 'f':
              case 'F':
              case '0':
                parsed_field = Qfalse;
                break;
              default:
                RAISE_WITH_LOCATION(
                  meta->current_row,
                  meta->current_col,
                  field_str,
                  "Bad Boolean value. Valid values are strings where the first character is T/t/1 for true or F/f/0 for false."
                );
            }
            break;
          default:
            RAISE_WITH_LOCATION(
              meta->current_row,
              meta->current_col,
              field_str,
              "Unknown deserializer '%c'.",
              row_conversion
            );
        }
      } else { /* No conversion happens */
        parsed_field = rb_str_new(field_str, field_size); /* field */
      }
    }

    /* Assign the value to appropriate hash key if parsing into Hash */
    if (meta->row_as_hash) {
      if (meta->current_col >= meta->num_columns) {
        RAISE_WITH_LOCATION(
          meta->current_row,
          meta->current_col,
          field_str,
          "There are at least %d columns in a row, which is beyond the number of provided column names (%d).",
          (int)meta->current_col + 1,
          (int)meta->num_columns
        );
      } else {
        rb_hash_aset(meta->last_entry, meta->column_names[meta->current_col], parsed_field); /* last_entry[column_names[current_col]] = field */
      }
    } else { /* Parse into Array */
      rb_ary_push(meta->last_entry, parsed_field); /* last_entry << field */
    }
  }

  /* Increment column counter */
  meta->current_col++;
  return;
}

/* This procedure is called for every line ending */
void end_of_line_callback(int last_char, void * data) {
  struct rcsv_metadata * meta = (struct rcsv_metadata *) data;

  /* If filters didn't match, current row parsing is reverted */
  if (meta->skip_current_row) {
    /* Do we wanna GC? */
    meta->skip_current_row = false;
  } else {
    if (rb_block_given_p()) { /* STREAMING */
      rb_yield(meta->last_entry);
    } else {
      rb_ary_push(*(meta->result), meta->last_entry);
    }
  }

  /* Re-initialize last_entry unless EOF reached */
  if (last_char != -1) {
    if (meta->row_as_hash) {
      meta->last_entry = rb_hash_new(); /* {} */
    } else {
      meta->last_entry = rb_ary_new(); /* [] */
    }
  }

  /* Resetting column counter */
  meta->current_col = 0;

  /* Incrementing row counter */
  meta->current_row++;
  return;
}

void custom_end_of_line_callback(int last_char, void * data) {
  struct rcsv_metadata * meta = (struct rcsv_metadata *) data;

  if (!meta->skip_current_row) {
  }
}

/* C API */

/* The main method that handles parsing */
static VALUE rb_rcsv_raw_parse(int argc, VALUE * argv, VALUE self) {
  struct rcsv_metadata meta;
  VALUE csvio, csvstr, buffer_size, options, option;

  struct csv_parser cp;
  unsigned char csv_options = CSV_STRICT_FINI | CSV_APPEND_NULL;
  char * csv_string;
  size_t csv_string_len;
  int error;
  size_t i = 0;

  /* Setting up some sane defaults */
  meta.row_as_hash = false;
  meta.skip_current_row = false;
  meta.num_columns = 0;
  meta.current_col = 0;
  meta.current_row = 0;
  meta.offset_rows = 0;
  meta.num_only_rows = 0;
  meta.num_row_defaults = 0;
  meta.num_row_conversions = 0;
  meta.only_rows = NULL;
  meta.row_defaults = NULL;
  meta.row_conversions = NULL;
  meta.column_names = NULL;
  meta.result = (VALUE[]){rb_ary_new()}; /* [] */

  /* csvio is required, options is optional (pun intended) */
  rb_scan_args(argc, argv, "11", &csvio, &options);

  /* options ||= nil */
  if (NIL_P(options)) {
    options = rb_hash_new();
  }

  buffer_size = rb_hash_aref(options, ID2SYM(rb_intern("buffer_size")));

  /* By default, parsing is strict */
  option = rb_hash_aref(options, ID2SYM(rb_intern("nostrict")));
  if (!option || (option == Qnil)) {
    csv_options |= CSV_STRICT;
  }

  /* Try to initialize libcsv */
  if (csv_init(&cp, csv_options) == -1) {
    rb_raise(rcsv_parse_error, "Couldn't initialize libcsv");
  }

  /* By default, parse as Array of Arrays */
  option = rb_hash_aref(options, ID2SYM(rb_intern("row_as_hash")));
  if (option && (option != Qnil)) {
    meta.row_as_hash = true;
  }

  /* :col_sep sets the column separator, default is comma (,) */
  option = rb_hash_aref(options, ID2SYM(rb_intern("col_sep")));
  if (option != Qnil) {
    csv_set_delim(&cp, (unsigned char)*StringValuePtr(option));
  }

  /* Specify how many rows to skip from the beginning of CSV */
  option = rb_hash_aref(options, ID2SYM(rb_intern("offset_rows")));
  if (option != Qnil) {
    meta.offset_rows = (size_t)NUM2INT(option);
  }

  /* :only_rows is a string mask where row is only parsed
     if its fields match those in the passed array.
     [nil, nil, "ABC"] skips all rows where 3rd column isn't equal to "ABC" */
  option = rb_hash_aref(options, ID2SYM(rb_intern("only_rows")));
  if (option != Qnil) {
    meta.num_only_rows = (size_t)RARRAY_LEN(option);
    meta.only_rows = (char **)malloc(meta.num_only_rows * sizeof(char *));

    for (i = 0; i < meta.num_only_rows; i++) {
      VALUE only_row = rb_ary_entry(option, i);
      if (only_row == Qnil) {
        meta.only_rows[i] = NULL;
      } else {
        meta.only_rows[i] = StringValueCStr(only_row);
      }
    }
  }

  /* :row_defaults is an array of default values that are assigned to fields containing empty strings
     according to matching field positions */
  option = rb_hash_aref(options, ID2SYM(rb_intern("row_defaults")));
  if (option != Qnil) {
    meta.num_row_defaults = RARRAY_LEN(option);
    meta.row_defaults = (VALUE*)malloc(meta.num_row_defaults * sizeof(VALUE*));

    for (i = 0; i < meta.num_row_defaults; i++) {
      VALUE row_default = rb_ary_entry(option, i);
      meta.row_defaults[i] = row_default;
    }
  }

  /* :row_conversions specifies Ruby types that CSV field values should be converted into.
     Each char of row_conversions string represents Ruby type for CSV field with matching position. */
  option = rb_hash_aref(options, ID2SYM(rb_intern("row_conversions"))); 
  if (option != Qnil) {
    meta.num_row_conversions = RSTRING_LEN(option);
    meta.row_conversions = StringValuePtr(option);
  }

 /* Column names should be declared explicitly when parsing fields as Hashes */
  if (meta.row_as_hash) { /* Only matters for hash results */
    option = rb_hash_aref(options, ID2SYM(rb_intern("column_names"))); 
    if (option == Qnil) {
      rb_raise(rcsv_parse_error, ":row_as_hash requires :column_names to be set.");
    } else {
      meta.last_entry = rb_hash_new();

      meta.num_columns = (size_t)RARRAY_LEN(option);
      meta.column_names = (VALUE*)malloc(meta.num_columns * sizeof(VALUE*));

      for (i = 0; i < meta.num_columns; i++) {
        meta.column_names[i] = rb_ary_entry(option, i);
      }
    }
  } else {
    meta.last_entry = rb_ary_new();
  }

  while(true) {
    csvstr = rb_funcall(csvio, rb_intern("read"), 1, buffer_size);
    if ((csvstr == Qnil) || (RSTRING_LEN(csvstr) == 0)) { break; }

    csv_string = StringValuePtr(csvstr);
    csv_string_len = strlen(csv_string);

    /* Actual parsing and error handling */
    if (csv_string_len != csv_parse(&cp, csv_string, csv_string_len,
                                    &end_of_field_callback, &end_of_line_callback, &meta)) {
      error = csv_error(&cp);
      switch(error) {
        case CSV_EPARSE:
          rb_raise(rcsv_parse_error, "Error when parsing malformed data");
          break;
        case CSV_ENOMEM:
          rb_raise(rcsv_parse_error, "No memory");
          break;
        case CSV_ETOOBIG:
          rb_raise(rcsv_parse_error, "Field data is too large");
          break;
        case CSV_EINVALID:
          rb_raise(rcsv_parse_error, "%s", (const char *)csv_strerror(error));
        break;
        default:
          rb_raise(rcsv_parse_error, "Failed due to unknown reason");
      }
    }
  }

  /* Flushing libcsv's buffer and freeing up allocated memory */
  csv_fini(&cp, &end_of_field_callback, &end_of_line_callback, &meta);
  csv_free(&cp);

  if (meta.only_rows != NULL) {
    free(meta.only_rows);
  }

  if (meta.row_defaults != NULL) {
    free(meta.row_defaults);
  }

  if (meta.column_names != NULL) {
    free(meta.column_names);
  }

  /* Remove the last row if it's empty. That happens if CSV file ends with a newline. */
  if (RARRAY_LEN(*(meta.result)) && /* meta.result.size != 0 */
      RARRAY_LEN(rb_ary_entry(*(meta.result), -1)) == 0) {
    rb_ary_pop(*(meta.result));
  }

  if (rb_block_given_p()) {
    return Qnil; /* STREAMING */
  } else {
    return *(meta.result); /* Return accumulated result */
  }
}


/* Define Ruby API */
void Init_rcsv(void) {
  VALUE klass = rb_define_class("Rcsv", rb_cObject); /* class Rcsv; end */

  /* Error is initialized through static variable in order to access it from rb_rcsv_raw_parse */
  rcsv_parse_error = rb_define_class_under(klass, "ParseError", rb_eStandardError);

  /* def Rcsv.raw_parse; ...; end */
  rb_define_singleton_method(klass, "raw_parse", rb_rcsv_raw_parse, -1);
}
