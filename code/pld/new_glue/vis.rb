require 'date'
#require 'pp'

SIG_CHRS = (33..47).map(&:chr) + (58..126).map(&:chr)

if ARGV.empty?
  puts "Usage: ruby vis.rb <basename>"
  exit(1)
end

basename = ARGV[0]

output = File.read("#{basename}.so")

orders = output.scan(/ORDER:(.*)/)
  .map { |(line)| line.strip.sub(/;$/,'').split(',').map { |field| field.strip }.reject { |field| field =~ /^%\d+$/ } }

steps = output.scan(/(\d{4}):(.*)/).map { |(t,vec)| [t.to_i, vec.strip.split(/ /).join] }

def assign_signal(name)
  if SIG_CHRS.empty?
    raise "Too many signals"
  end

  SIG_CHRS.shift
end

def determine_signal_width(output, signame)
  width = output.scan(/FIELD ([^\s]+)\s?=\s?\[([^\]]+)\];/)
    .map { |(name, signals)| [name, signals.split(',').length] }
    .find { |(name, width)| name.eql?(signame) }

  if width.nil?
    1
  else
    width.last
  end
end


def build_signals(output, orders)
  h = {}

  orders.each do |signames|
    signames.each do |signame|
      h[signame] = { 
        name: signame, 
        id: assign_signal(signame), 
        width: determine_signal_width(output, signame) 
      }
    end
  end

  h
end

#pp build_signals(output, orders)

File.open("#{basename}.vcd", 'w') do |vcd|
  signals = build_signals(output, orders)  

  vcd.puts("$version hacked together by a script")
  vcd.puts("$date #{DateTime.now.to_s} $end")
  vcd.puts("$timescale 1ns $end")
  vcd.puts("$scope module TOP $end")
  
  orders.each_with_index do |signames, idx|
    vcd.puts("$scope module ORDER#{idx} $end")
    signames.each do |signame|
      vcd.puts("$var wire #{signals[signame][:width]} #{signals[signame][:id]} #{signame} $end")
    end
    vcd.puts("$upscope $end")
  end

  vcd.puts("$upscope $end")
  vcd.puts("$enddefinitions")
  vcd.puts("$end")

  c_on = false

  vcd.puts("#0")
  steps.each do |(stepnum, step)|
    vcd.puts("##{stepnum * 100}")

    values = step.strip.split(//)

    (0...signals.length).each do |idx|
      out = ""

      signal_name = orders[0][idx]
      signal = signals[signal_name]

      if signal[:width] > 1
        out += 'b'
      end

      sig_vals = values.shift(signal[:width])
      out += sig_vals.inject("") do |s, sig_val| 
        if sig_val.eql?('C')
          if c_on
            clk_sig = '1'
          else
            clk_sig = '0'
          end
          c_on = !c_on
          s + clk_sig
        elsif sig_val.eql?('L') || sig_val.eql?('0')
          s + '0'
        elsif sig_val.eql?('H') || sig_val.eql?('1')
          s + '1'
        elsif sig_val.eql?('Z')
          s + 'z'
        else
          # default to X
          s + 'x'
        end
      end
      
      if signal[:width] > 1
        out += ' '
      end

      vcd.puts("#{out}#{signal[:id]}")
    end
  end
end



