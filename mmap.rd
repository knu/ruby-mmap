=begin
= Mmap

((<Download|URL:ftp://moulon.inra.fr/pub/ruby/>))

#^
The Mmap class implement memory-mapped file objects

=== WARNING
=== The variables $' and $` are not available with gsub! and sub!
#^

== SuperClass

Object

== Included Modules

* Comparable
* Enumerable
# class Mmap
#  include Comparable
#  include Enumerable
#  class << self
== Class Methods

--- lockall(flag)
      disable paging of all pages mapped. ((|flag|)) can be 
      ((|Mmap::MCL_CURRENT|)) or ((|Mmap::MCL_FUTURE|))

--- new(file, mode = "r", protection = Mmap::MAP_SHARED, options = {})
--- new(nil, length, protection = Mmap::MAP_SHARED, options = {})
      create a new Mmap object

        : ((|file|))
            Pathname of the file, if ((|nil|)) is given an anonymous map
            is created ((|Mmanp::MAP_ANON|))

        : ((|mode|))
            Mode to open the file, it can be "r", "w", "rw", "a"

        : ((|protection|))
            specify the nature of the mapping

               : ((|Mmap::MAP_SHARED|))
                   Creates a mapping that's shared with all other processes 
                   mapping the same areas of the file. 
                   The default value is ((|Mmap::MAP_SHARED|))

               : ((|Mmap::MAP_PRIVATE|))
                   Creates a private copy-on-write mapping, so changes to the
                   contents of the mmap object will be private to this process

        : ((|options|))
            Hash. If one of the options ((|length|)) or ((|offset|))
            is specified it will not possible to modify the size of
            the mapped file.

               : ((|length|))
                   Maps ((|length|)) bytes from the file

               : ((|offset|))
                   The mapping begin at ((|offset|))

               : ((|advice|))
                   The type of the access (see #madvise)


--- unlockall
     reenable paging

#  end
== Methods

--- extend(count)
     add ((|count|)) bytes to the file (i.e. pre-extend the file) 

--- madvise(advice)
     ((|advice|)) can have the value ((|Mmap::MADV_NORMAL|)),
     ((|Mmap::MADV_RANDOM|)), ((|Mmap::MADV_SEQUENTIAL|)),
     ((|Mmap::MADV_WILLNEED|)), ((|Mmap::MADV_DONTNEED|))

--- mprotect(mode)
     change the mode, value must be "r", "w" or "rw"

--- mlock
     disable paging

--- msync
--- flush
     flush the file

--- munlock
     reenable paging

--- munmap
     terminate the association

=== Other methods with the same syntax than for the class String


--- self == other 

--- self > other 

--- self >= other 

--- self < other 

--- self <= other 

--- self === other 

--- self << other 

--- self =~ other 

--- self[nth] 

--- self[start..last] 

--- self[start, length] 

--- self[nth] = val 

--- self[start..last] = val 

--- self[start, len] = val 

--- self <=> other 

--- <<(other) 

--- casecmp(other)   >= 1.7.1

--- concat(other) 

--- capitalize! 

--- chop! 

--- chomp!([rs]) 

--- count(o1 [, o2, ...])

--- crypt(salt) 

--- delete!(str) 

--- downcase! 

--- each_byte {|char|...} 

--- each([rs]) {|line|...} 

--- each_line([rs]) {|line|...} 

--- empty? 

--- freeze 

--- frozen 

--- gsub!(pattern, replace) 

--- gsub!(pattern) {|str|...}

--- include?(other) 

--- index(substr[, pos]) 

--- insert(index, str) >= 1.7.1

--- length 

--- reverse! 

--- rindex(substr[, pos]) 

--- scan(pattern) 

--- scan(pattern) {|str| ...} 

--- size 

--- slice

--- slice!

--- split([sep[, limit]]) 

--- squeeze!([str]) 

--- strip! 

--- sub!(pattern, replace) 

--- sub!(pattern) {|str| ...} 

--- sum([bits]) 

--- swapcase! 

--- tr!(search, replace) 

--- tr_s!(search, replace) 

--- upcase! 

=end
