require "mkmf"
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
\t$(CC) -static /tmp/a.c $(OBJS) $(CPPFLAGS) $(DLDFLAGS) -lruby #{CONFIG["LIBS"]} $(LIBS) $(LOCAL_LIBS)
\t@-rm /tmp/a.c a.out
   EOT
ensure
   make.close
end

