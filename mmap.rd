=begin
= Mmap

((<Download|URL:ftp://moulon.inra.fr/pub/ruby/>))

The Mmap class implement memory-mapped file objects

=== WARNING
((*The variables $' and $` are not available with gsub! and sub!*))

== SuperClass

Object

== Included Modules

* Comparable
* Enumerable

== Class Methods

--- new(file, [mode [, protection [, options]]])
      create a new object

        : ((|file|))
            Pathname of the file

        : ((|mode|))
            Mode to open the file, it can be "r", "w" or "rw"

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

== Methods

--- extend(count)
     add ((|count|)) bytes to the file (i.e. pre-extend the file) 

--- madvise(advice)
     ((|advice|)) can have the value ((|Mmap::MADV_NORMAL|)),
     ((|Mmap::MADV_RANDOM|)), ((|Mmap::MADV_SEQUENTIAL|)),
     ((|Mmap::MADV_WILLNEED|)), ((|Mmap::MADV_DONTNEED|))

--- mprotect(mode)
     change the mode, value must be "r", "w" or "rw"

--- msync
--- flush
     flush the file

--- munmap
     terminate the association

=== Other methods with the same syntax than the methods of ((|String|))

self == other 

self > other 

self >= other 

self < other 

self <= other 

self === other 

self << other 

self =~ other 

self[nth] 

self[start..last] 

self[start, length] 

self[nth] = val 

self[start..last] = val 

self[start, len] = val 

self <=> other 

<<(other) 

concat(other) 

capitalize! 

chop! 

chomp!([rs]) 

crypt(salt) 

delete!(str) 

downcase! 

each_byte {|char|...} 

each([rs]) {|line|...} 

each_line([rs]) {|line|...} 

empty? 

freeze 

frozen 

gsub!(pattern, replace) 

gsub!(pattern) {...}

include?(other) 

index(substr[, pos]) 

length 

reverse! 

rindex(substr[, pos]) 

scan(pattern) 

scan(pattern) {...} 

size 

split([sep[, limit]]) 

squeeze!([str]) 

strip! 

sub!(pattern, replace) 

sub!(pattern) {...} 

sum([bits]) 

swapcase! 

tr!(search, replace) 

tr_s!(search, replace) 

upcase! 

=end
