#!/usr/bin/ruby
$LOAD_PATH.unshift *%w{.. . tests}
require 'mmap'
require 'runit/testcase'
require 'runit/cui/testrunner'

$mmap, $str = nil, nil

class TestMmap < RUNIT::TestCase
   def internal_init
      $mmap.unmap if $mmap
      mmap = open("tmp/mmap", "w")
      $str = <<-EOT

 some randomly generated text

 well, in reality not really, really random
      EOT
      mmap.print $str
      mmap.close
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
      assert_equal($mmap.scan(/random/), $str.scan(/random/), "<scan>")
      assert_equal($mmap.index("really"), $str.index("really"), "<index>")
      assert_equal($mmap.rindex("really"), $str.rindex("really"), "<rindex>")
      ('a' .. 'z').each do |i|
	 assert_equal($mmap.index(i), $str.index(i), "<index>")
	 assert_equal($mmap.rindex(i), $str.rindex(i), "<rindex>")
      end
      assert_equal($mmap.sub!(/real/, 'XXXX'), $str.sub!(/real/, 'XXXX'), "<sub!>")
      assert_equal($mmap.to_str, $str, "<after sub!>")
      assert_equal($mmap.gsub!(/real/, 'XXXX'), $str.gsub!(/real/, 'XXXX'), "<sub!>")
      assert_equal($mmap.to_str, $str, "<after gsub!>")
      assert_equal($mmap.gsub!(/YYYY/, 'XXXX'), $str.gsub!(/YYYY/, 'XXXX'), "<sub!>")
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
      internal_modify(:tr!, 'abcdefghi', '123456789')
      internal_modify(:tr_s!, 'jklmnopqr', '123456789')
   end


end

RUNIT::CUI::TestRunner.run(TestMmap.suite)
