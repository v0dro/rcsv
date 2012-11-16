require './rcsv'

r = Rcsv.parse(['1,2,3,4,5,abc,"def"', '5,,7,8,9,"yo, kmon", foo'].join("\n"), :col_sep => ',')

puts r.inspect
