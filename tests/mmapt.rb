#!/usr/bin/ruby
$LOAD_PATH.unshift *%w{.. . tests}
require 'mmap'
require 'ftools'
begin
   require 'test/unit'
   Inh = Test::Unit
rescue LoadError
   require 'runit/testcase'
   require 'runit/cui/testrunner'
   Inh = RUNIT
end

$mmap, $str = nil, nil

class TestMmap < Inh::TestCase
   def internal_init
      $mmap.unmap if $mmap
      file = "mmap.c"
      file = "../mmap.c" unless File.exist? file
      File.syscopy file, "tmp/mmap"
      $str = File.readlines("tmp/mmap", nil)[0]
      assert_kind_of(Mmap, $mmap = Mmap.new("tmp/mmap", "rw"), "<open>")
   end

   def test_00_init
      internal_init
      assert_equal($mmap.length, $str.length, "<lenght>")
   end

   def test_01_aref
      max = $str.size * 2
      72.times do
	 ran1 = rand(max)
	 assert_equal($mmap[ran1], $str[ran1], "<aref>");
	 assert_equal($mmap[-ran1], $str[-ran1], "<aref>");
	 ran2 = rand(max)
	 assert_equal($mmap[ran1, ran2], $str[ran1, ran2], "<double aref>");
	 assert_equal($mmap[-ran1, ran2], $str[-ran1, ran2], "<double aref>");
	 assert_equal($mmap[ran1, -ran2], $str[ran1, -ran2], "<double aref>");
	 assert_equal($mmap[-ran1, -ran2], $str[-ran1, -ran2], "<double aref>");
	 assert_equal($mmap[ran1 .. ran2], $str[ran1 .. ran2], "<double aref>");
	 assert_equal($mmap[-ran1 .. ran2], $str[-ran1 .. ran2], "<double aref>");
	 assert_equal($mmap[ran1 .. -ran2], $str[ran1 .. -ran2], "<double aref>");
	 assert_equal($mmap[-ran1 .. -ran2], $str[-ran1 .. -ran2], "<double aref>");
      end
      assert_equal($mmap[/random/], $str[/random/], "<aref regexp>")
      assert_equal($mmap[/real/], $str[/real/], "<aref regexp>")
      assert_equal($mmap[/none/], $str[/none/], "<aref regexp>")
   end

   def internal_aset(a, b = nil, c = true)
      access = if b
		  repl = ''
		  rand(12).times do
	              repl << (65 + rand(25))
                  end
		  if c 
		     "[a, b] = '#{repl}'"
		  else
		     "[a .. b] = '#{repl}'"
		  end
	       else
		  "[a] = #{(65 + rand(25))}"
	       end
      begin
	 eval "$str#{access}"
      rescue IndexError, RangeError
	 begin
	    eval "$mmap#{access}"
	 rescue IndexError, RangeError
	 else
	    assert_fail("*must* fail with IndexError")
	 end
      else
	 eval "$mmap#{access}"
      end
      assert_equal($mmap.to_str, $str, "<internal aset>")
   end

   def test_02_aset
      max = $str.size * 2
      72.times do
	 ran1 = rand(max)
	 internal_aset(ran1)
	 internal_aset(-ran1)
	 ran2 = rand(max)
	 internal_aset(ran1, ran2)
	 internal_aset(ran1, -ran2)
	 internal_aset(-ran1, ran2)
	 internal_aset(-ran1, -ran2)
	 internal_aset(ran1, ran2, false)
	 internal_aset(ran1, -ran2, false)
	 internal_aset(-ran1, ran2, false)
	 internal_aset(-ran1, -ran2, false)
      end
      internal_init
   end

   def internal_slice(a, b = nil, c = true)
      access = if b
		  if c 
		     ".slice!(a, b)"
		  else
		     ".slice!(a .. b)"
		  end
	       else
		  ".slice!(a)"
	       end
      begin
	 eval "$str#{access}"
      rescue IndexError, RangeError
	 begin
	    eval "$mmap#{access}"
	 rescue IndexError, RangeError
	 else
	    assert_fail("*must* fail with IndexError")
	 end
      else
	 eval "$mmap#{access}"
      end
      assert_equal($mmap.to_str, $str, "<internal aset>")
   end

   def test_03_slice
      max = $str.size * 2
      72.times do
	 ran1 = rand(max)
	 internal_slice(ran1)
	 internal_slice(-ran1)
	 ran2 = rand(max)
	 internal_slice(ran1, ran2)
	 internal_slice(ran1, -ran2)
	 internal_slice(-ran1, ran2)
	 internal_slice(-ran1, -ran2)
	 internal_slice(ran1, ran2, false)
	 internal_slice(ran1, -ran2, false)
	 internal_slice(-ran1, ran2, false)
	 internal_slice(-ran1, -ran2, false)
      end
      internal_init
   end

   def test_04_reg
      assert_equal($mmap.scan(/include/), $str.scan(/include/), "<scan>")
      assert_equal($mmap.index("rb_raise"), $str.index("rb_raise"), "<index>")
      assert_equal($mmap.rindex("rb_raise"), $str.rindex("rb_raise"), "<rindex>")
      ('a' .. 'z').each do |i|
	 assert_equal($mmap.index(i), $str.index(i), "<index>")
	 assert_equal($mmap.rindex(i), $str.rindex(i), "<rindex>")
      end
      $mmap.sub!(/GetMmap/, 'XXXX'); $str.sub!(/GetMmap/, 'XXXX')
      assert_equal($mmap.to_str, $str, "<after sub!>")
      $mmap.gsub!(/GetMmap/, 'XXXX'); $str.gsub!(/GetMmap/, 'XXXX')
      assert_equal($mmap.to_str, $str, "<after gsub!>")
      $mmap.gsub!(/YYYY/, 'XXXX'); $str.gsub!(/YYYY/, 'XXXX')
      assert_equal($mmap.to_str, $str, "<after gsub!>")
      assert_equal($mmap.split(/\w+/), $str.split(/\w+/), "<split>")
      assert_equal($mmap.split(/\W+/), $str.split(/\W+/), "<split>")
      assert_equal($mmap.crypt("abc"), $str.crypt("abc"), "<crypt>")
      internal_init
   end

   def internal_modify idmod, *args
      if res = $str.method(idmod)[*args]
	 assert_equal($mmap.method(idmod)[*args].to_str, res, "<#{idmod}>")
      else
	 assert_equal($mmap.method(idmod)[*args], res, "<#{idmod}>")
      end
   end

   def test_05_modify
      internal_modify(:reverse!)
      internal_modify(:upcase!)
      internal_modify(:downcase!)
      internal_modify(:capitalize!)
      internal_modify(:swapcase!)
      internal_modify(:strip!)
      internal_modify(:chop!)
      internal_modify(:chomp!)
      internal_modify(:squeeze!)
      internal_modify(:tr!, 'abcdefghi', '123456789')
      internal_modify(:tr_s!, 'jklmnopqr', '123456789')
      internal_modify(:delete!, 'A-Z')
   end

   def test_06_iterate
      internal_init
      mmap = []; $mmap.each {|l| mmap << l}
      str = []; $str.each {|l| str << l}
      assert_equal(mmap, str, "<each>")
      mmap = []; $mmap.each_byte {|l| mmap << l}
      str = []; $str.each_byte {|l| str << l}
      assert_equal(mmap, str, "<each_byte>")
   end

end

if defined?(RUNIT)
   RUNIT::CUI::TestRunner.run(TestMmap.suite)
end

