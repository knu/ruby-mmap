#!/usr/bin/ruby
#--
# don't use it's for gem
$LOAD_PATH.each do |p|
   if p.gsub!(%r{/ext\Z}, '/test') &&
         File.exists?(p + '/mmapt.rb')
      load(p + '/mmapt.rb')
      break
   end
end

