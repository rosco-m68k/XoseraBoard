def col(addr); ((addr >> 1) &  0b00000000000000111111111); end
def bank(addr);(((addr >> 1) & 0b10000000000000000000000) >> 22); end
def row(addr); (((addr >> 1) & 0b01111111111111000000000)) >> 9; end
def dram(addr); { bank: bank(addr), row: row(addr), col: col(addr) }; end

(0..8388607 * 2).step(256).map { |a| ["0x%08x" % a, dram(a)] }.each { |d| p d }

