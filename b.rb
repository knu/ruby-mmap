#!/usr/bin/ruby
require "mmap"
m = Mmap.new("aa", "rw")
m.gsub!(/(.)/, '(\&)')

