# The Mmap class implement memory-mapped file objects
# 
# === WARNING
# === The variables $' and $` are not available with gsub! and sub!
class Mmap
include Comparable
include Enumerable
class << self

#disable paging of all pages mapped. <em>flag</em> can be 
#<em>Mmap::MCL_CURRENT</em> or <em>Mmap::MCL_FUTURE</em>
#
def  lockall(flag)
end

#create a new Mmap object
#
#* <em>file</em>
#    Pathname of the file, if <em>nil</em> is given an anonymous map
#    is created <em>Mmanp::MAP_ANON</em>
#
#* <em>mode</em>
#    Mode to open the file, it can be "r", "w", "rw", "a"
#
#* <em>protection</em>
#    specify the nature of the mapping
#
#  * <em>Mmap::MAP_SHARED</em>
#      Creates a mapping that's shared with all other processes 
#      mapping the same areas of the file. 
#      The default value is <em>Mmap::MAP_SHARED</em>
#
#  * <em>Mmap::MAP_PRIVATE</em>
#      Creates a private copy-on-write mapping, so changes to the
#      contents of the mmap object will be private to this process
#
#* <em>options</em>
#    Hash. If one of the options <em>length</em> or <em>offset</em>
#    is specified it will not possible to modify the size of
#    the mapped file.
#
#  * <em>length</em>
#      Maps <em>length</em> bytes from the file
#
#  * <em>offset</em>
#      The mapping begin at <em>offset</em>
#
#  * <em>advice</em>
#      The type of the access (see #madvise)
#
#
def  new(file, mode = "r", protection = Mmap::MAP_SHARED, options = {})
end

#reenable paging
#
def  unlockall
end
end

#add <em>count</em> bytes to the file (i.e. pre-extend the file) 
#
def  extend(count)
end

#<em>advice</em> can have the value <em>Mmap::MADV_NORMAL</em>,
#<em>Mmap::MADV_RANDOM</em>, <em>Mmap::MADV_SEQUENTIAL</em>,
#<em>Mmap::MADV_WILLNEED</em>, <em>Mmap::MADV_DONTNEED</em>
#
def  madvise(advice)
end

#change the mode, value must be "r", "w" or "rw"
#
def  mprotect(mode)
end

#disable paging
#
def  mlock
end

#flush the file
#
def  msync
end
#same than <em> msync</em>
def  flush
end

#reenable paging
#
def  munlock
end

#terminate the association
#
#=== Other methods with the same syntax than for the class String
#
#
def  munmap
end

#
def  self == other 
end

#
def  self > other 
end

#
def  self >= other 
end

#
def  self < other 
end

#
def  self <= other 
end

#
def  self === other 
end

#
def  self << other 
end

#
def  self =~ other 
end

#
def  self[nth] 
end

#
def  self[start..last] 
end

#
def  self[start, length] 
end

#
def  self[nth] = val 
end

#
def  self[start..last] = val 
end

#
def  self[start, len] = val 
end

#
def  self <=> other 
end

#
def  <<(other) 
end

#
def  casecmp(other)   >= 1.7.1
end

#
def  concat(other) 
end

#
def  capitalize! 
end

#
def  chop! 
end

#
def  chomp!([rs]) 
end

#
def  count(o1 [, o2, ...])
end

#
def  crypt(salt) 
end

#
def  delete!(str) 
end

#
def  downcase! 
end

#
def  each_byte  
yield char
end

#
def  each([rs])  
yield line
end

#
def  each_line([rs])  
yield line
end

#
def  empty? 
end

#
def  freeze 
end

#
def  frozen 
end

#
def  gsub!(pattern, replace) 
end

#
def  gsub!(pattern) 
yield str
end

#
def  include?(other) 
end

#
def  index(substr[, pos]) 
end

#
def  insert(index, str) >= 1.7.1
end

#
def  length 
end

#
def  reverse! 
end

#
def  rindex(substr[, pos]) 
end

#
def  scan(pattern) 
end

#
def  scan(pattern)  
yield str
end

#
def  size 
end

#
def  slice
end

#
def  slice!
end

#
def  split([sep[, limit]]) 
end

#
def  squeeze!([str]) 
end

#
def  strip! 
end

#
def  sub!(pattern, replace) 
end

#
def  sub!(pattern)  
yield str
end

#
def  sum([bits]) 
end

#
def  swapcase! 
end

#
def  tr!(search, replace) 
end

#
def  tr_s!(search, replace) 
end
