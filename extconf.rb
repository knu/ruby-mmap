#!/usr/bin/ruby
ARGV.collect! {|x| x.sub(/^--with-mmap-prefix=/, "--with-mmap-dir=") }

require 'mkmf'


def resolve(key)
   name = key.dup
   true while name.gsub!(/\$\((\w+)\)/) { CONFIG[$1] }
   name
end

if ! find_library(resolve(CONFIG["LIBRUBY"]).sub(/^lib(.*)\.\w+\z/, '\\1'), 
                  "ruby_init", resolve(CONFIG["archdir"]))
   raise "can't find -lruby"
end

dir_config("mmap")

create_makefile "mmap"

begin
   make = open("Makefile", "a")
   make.puts "\ntest: $(DLLIB)"
   Dir.foreach('tests') do |x|
      next if /^\./ =~ x || /(_\.rb|~)$/ =~ x
      next if FileTest.directory?(x)
      make.print "\truby tests/#{x}\n"
   end
   make.print <<-EOT

unknown: $(DLLIB)
\t@echo "main() {}" > /tmp/a.c
\t$(CC) -static /tmp/a.c $(OBJS) $(CPPFLAGS) $(DLDFLAGS) #{CONFIG["LIBS"]} $(LIBS) $(LOCAL_LIBS)
\t@-rm /tmp/a.c a.out

%.html: %.rd
\trd2 $< > ${<:%.rd=%.html}

   EOT
   make.print "HTML = mmap.html"
   docs = Dir['docs/*.rd']
   docs.each {|x| make.print " \\\n\t#{x.sub(/\.rd$/, '.html')}" }
   make.print "\n\nRDOC = mmap.rd"
   docs.each {|x| make.print " \\\n\t#{x}" }
   make.puts
   make.print <<-EOF

rdoc: docs/doc/index.html

docs/doc/index.html: $(RDOC)
\t@-(cd docs; $(RUBY) b.rb mmap; rdoc mmap.rb)

rd2: html

html: $(HTML)

   EOF
ensure
   make.close
end

