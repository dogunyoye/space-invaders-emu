#include <iomanip>
#include <iostream>
#include <sstream>
#include <cstdint>

#include "Disassembler8080.h"

// Builds "0xNNNN MNEMONIC" without touching stdout.
std::string disassemble8080_str(const uint8_t *codebuffer, int pc, int *out_len) {
    const uint8_t *code = &codebuffer[pc];
    int opbytes = 1;

    std::ostringstream out;
    // Print current Program Counter in hex
    out << "0x" << std::setfill('0') << std::setw(4) << std::hex << pc << " ";
    out << std::hex << std::setfill('0');

    uint8_t opcode = *code;

    switch (opcode) {
        case 0x00: out << "NOP"; break;
        case 0x01: out << "LXI    B,#$" << std::setw(4) << ((code[2] << 8) | code[1]); opbytes = 3; break;
        case 0x02: out << "STAX   B"; break;
        case 0x03: out << "INX    B"; break;
        case 0x04: out << "INR    B"; break;
        case 0x05: out << "DCR    B"; break;
        case 0x06: out << "MVI    B,#$" << std::setw(2) << (int)code[1]; opbytes = 2; break;
        case 0x07: out << "RLC"; break;
        case 0x08: out << "NOP"; break;
        case 0x09: out << "DAD    B"; break;
        case 0x0a: out << "LDAX   B"; break;
        case 0x0b: out << "DCX    B"; break;
        case 0x0c: out << "INR    C"; break;
        case 0x0d: out << "DCR    C"; break;
        case 0x0e: out << "MVI    C,#$" << std::setw(2) << (int)code[1]; opbytes = 2; break;
        case 0x0f: out << "RRC"; break;

        case 0x10: out << "NOP"; break;
        case 0x11: out << "LXI    D,#$" << std::setw(4) << ((code[2] << 8) | code[1]); opbytes = 3; break;
        case 0x12: out << "STAX   D"; break;
        case 0x13: out << "INX    D"; break;
        case 0x14: out << "INR    D"; break;
        case 0x15: out << "DCR    D"; break;
        case 0x16: out << "MVI    D,#$" << std::setw(2) << (int)code[1]; opbytes = 2; break;
        case 0x17: out << "RAL"; break;
        case 0x18: out << "NOP"; break;
        case 0x19: out << "DAD    D"; break;
        case 0x1a: out << "LDAX   D"; break;
        case 0x1b: out << "DCX    D"; break;
        case 0x1c: out << "INR    E"; break;
        case 0x1d: out << "DCR    E"; break;
        case 0x1e: out << "MVI    E,#$" << std::setw(2) << (int)code[1]; opbytes = 2; break;
        case 0x1f: out << "RAR"; break;

        case 0x20: out << "NOP"; break; // RIM on 8085
        case 0x21: out << "LXI    H,#$" << std::setw(4) << ((code[2] << 8) | code[1]); opbytes = 3; break;
        case 0x22: out << "SHLD   $" << std::setw(4) << ((code[2] << 8) | code[1]); opbytes = 3; break;
        case 0x23: out << "INX    H"; break;
        case 0x24: out << "INR    H"; break;
        case 0x25: out << "DCR    H"; break;
        case 0x26: out << "MVI    H,#$" << std::setw(2) << (int)code[1]; opbytes = 2; break;
        case 0x27: out << "DAA"; break;
        case 0x28: out << "NOP"; break;
        case 0x29: out << "DAD    H"; break;
        case 0x2a: out << "LHLD   $" << std::setw(4) << ((code[2] << 8) | code[1]); opbytes = 3; break;
        case 0x2b: out << "DCX    H"; break;
        case 0x2c: out << "INR    L"; break;
        case 0x2d: out << "DCR    L"; break;
        case 0x2e: out << "MVI    L,#$" << std::setw(2) << (int)code[1]; opbytes = 2; break;
        case 0x2f: out << "CMA"; break;

        case 0x30: out << "NOP"; break; // SIM on 8085
        case 0x31: out << "LXI    SP,#$" << std::setw(4) << ((code[2] << 8) | code[1]); opbytes = 3; break;
        case 0x32: out << "STA    $" << std::setw(4) << ((code[2] << 8) | code[1]); opbytes = 3; break;
        case 0x33: out << "INX    SP"; break;
        case 0x34: out << "INR    M"; break;
        case 0x35: out << "DCR    M"; break;
        case 0x36: out << "MVI    M,#$" << std::setw(2) << (int)code[1]; opbytes = 2; break;
        case 0x37: out << "STC"; break;
        case 0x38: out << "NOP"; break;
        case 0x39: out << "DAD    SP"; break;
        case 0x3a: out << "LDA    $" << std::setw(4) << ((code[2] << 8) | code[1]); opbytes = 3; break;
        case 0x3b: out << "DCX    SP"; break;
        case 0x3c: out << "INR    A"; break;
        case 0x3d: out << "DCR    A"; break;
        case 0x3e: out << "MVI    A,#$" << std::setw(2) << (int)code[1]; opbytes = 2; break;
        case 0x3f: out << "CMC"; break;

        case 0x40: out << "MOV    B,B"; break;
        case 0x41: out << "MOV    B,C"; break;
        case 0x42: out << "MOV    B,D"; break;
        case 0x43: out << "MOV    B,E"; break;
        case 0x44: out << "MOV    B,H"; break;
        case 0x45: out << "MOV    B,L"; break;
        case 0x46: out << "MOV    B,M"; break;
        case 0x47: out << "MOV    B,A"; break;
        case 0x48: out << "MOV    C,B"; break;
        case 0x49: out << "MOV    C,C"; break;
        case 0x4a: out << "MOV    C,D"; break;
        case 0x4b: out << "MOV    C,E"; break;
        case 0x4c: out << "MOV    C,H"; break;
        case 0x4d: out << "MOV    C,L"; break;
        case 0x4e: out << "MOV    C,M"; break;
        case 0x4f: out << "MOV    C,A"; break;

        case 0x50: out << "MOV    D,B"; break;
        case 0x51: out << "MOV    D,C"; break;
        case 0x52: out << "MOV    D,D"; break;
        case 0x53: out << "MOV    D,E"; break;
        case 0x54: out << "MOV    D,H"; break;
        case 0x55: out << "MOV    D,L"; break;
        case 0x56: out << "MOV    D,M"; break;
        case 0x57: out << "MOV    D,A"; break;
        case 0x58: out << "MOV    E,B"; break;
        case 0x59: out << "MOV    E,C"; break;
        case 0x5a: out << "MOV    E,D"; break;
        case 0x5b: out << "MOV    E,E"; break;
        case 0x5c: out << "MOV    E,H"; break;
        case 0x5d: out << "MOV    E,L"; break;
        case 0x5e: out << "MOV    E,M"; break;
        case 0x5f: out << "MOV    E,A"; break;

        case 0x60: out << "MOV    H,B"; break;
        case 0x61: out << "MOV    H,C"; break;
        case 0x62: out << "MOV    H,D"; break;
        case 0x63: out << "MOV    H,E"; break;
        case 0x64: out << "MOV    H,H"; break;
        case 0x65: out << "MOV    H,L"; break;
        case 0x66: out << "MOV    H,M"; break;
        case 0x67: out << "MOV    H,A"; break;
        case 0x68: out << "MOV    L,B"; break;
        case 0x69: out << "MOV    L,C"; break;
        case 0x6a: out << "MOV    L,D"; break;
        case 0x6b: out << "MOV    L,E"; break;
        case 0x6c: out << "MOV    L,H"; break;
        case 0x6d: out << "MOV    L,L"; break;
        case 0x6e: out << "MOV    L,M"; break;
        case 0x6f: out << "MOV    L,A"; break;

        case 0x70: out << "MOV    M,B"; break;
        case 0x71: out << "MOV    M,C"; break;
        case 0x72: out << "MOV    M,D"; break;
        case 0x73: out << "MOV    M,E"; break;
        case 0x74: out << "MOV    M,H"; break;
        case 0x75: out << "MOV    M,L"; break;
        case 0x76: out << "HLT"; break;
        case 0x77: out << "MOV    M,A"; break;
        case 0x78: out << "MOV    A,B"; break;
        case 0x79: out << "MOV    A,C"; break;
        case 0x7a: out << "MOV    A,D"; break;
        case 0x7b: out << "MOV    A,E"; break;
        case 0x7c: out << "MOV    A,H"; break;
        case 0x7d: out << "MOV    A,L"; break;
        case 0x7e: out << "MOV    A,M"; break;
        case 0x7f: out << "MOV    A,A"; break;

        case 0x80: out << "ADD    B"; break;
        case 0x81: out << "ADD    C"; break;
        case 0x82: out << "ADD    D"; break;
        case 0x83: out << "ADD    E"; break;
        case 0x84: out << "ADD    H"; break;
        case 0x85: out << "ADD    L"; break;
        case 0x86: out << "ADD    M"; break;
        case 0x87: out << "ADD    A"; break;
        case 0x88: out << "ADC    B"; break;
        case 0x89: out << "ADC    C"; break;
        case 0x8a: out << "ADC    D"; break;
        case 0x8b: out << "ADC    E"; break;
        case 0x8c: out << "ADC    H"; break;
        case 0x8d: out << "ADC    L"; break;
        case 0x8e: out << "ADC    M"; break;
        case 0x8f: out << "ADC    A"; break;

        case 0x90: out << "SUB    B"; break;
        case 0x91: out << "SUB    C"; break;
        case 0x92: out << "SUB    D"; break;
        case 0x93: out << "SUB    E"; break;
        case 0x94: out << "SUB    H"; break;
        case 0x95: out << "SUB    L"; break;
        case 0x96: out << "SUB    M"; break;
        case 0x97: out << "SUB    A"; break;
        case 0x98: out << "SBB    B"; break;
        case 0x99: out << "SBB    C"; break;
        case 0x9a: out << "SBB    D"; break;
        case 0x9b: out << "SBB    E"; break;
        case 0x9c: out << "SBB    H"; break;
        case 0x9d: out << "SBB    L"; break;
        case 0x9e: out << "SBB    M"; break;
        case 0x9f: out << "SBB    A"; break;

        case 0xa0: out << "ANA    B"; break;
        case 0xa1: out << "ANA    C"; break;
        case 0xa2: out << "ANA    D"; break;
        case 0xa3: out << "ANA    E"; break;
        case 0xa4: out << "ANA    H"; break;
        case 0xa5: out << "ANA    L"; break;
        case 0xa6: out << "ANA    M"; break;
        case 0xa7: out << "ANA    A"; break;
        case 0xa8: out << "XRA    B"; break;
        case 0xa9: out << "XRA    C"; break;
        case 0xaa: out << "XRA    D"; break;
        case 0xab: out << "XRA    E"; break;
        case 0xac: out << "XRA    H"; break;
        case 0xad: out << "XRA    L"; break;
        case 0xae: out << "XRA    M"; break;
        case 0xaf: out << "XRA    A"; break;

        case 0xb0: out << "ORA    B"; break;
        case 0xb1: out << "ORA    C"; break;
        case 0xb2: out << "ORA    D"; break;
        case 0xb3: out << "ORA    E"; break;
        case 0xb4: out << "ORA    H"; break;
        case 0xb5: out << "ORA    L"; break;
        case 0xb6: out << "ORA    M"; break;
        case 0xb7: out << "ORA    A"; break;
        case 0xb8: out << "CMP    B"; break;
        case 0xb9: out << "CMP    C"; break;
        case 0xba: out << "CMP    D"; break;
        case 0xbb: out << "CMP    E"; break;
        case 0xbc: out << "CMP    H"; break;
        case 0xbd: out << "CMP    L"; break;
        case 0xbe: out << "CMP    M"; break;
        case 0xbf: out << "CMP    A"; break;

        case 0xc0: out << "RNZ"; break;
        case 0xc1: out << "POP    B"; break;
        case 0xc2: out << "JNZ    $" << std::setw(4) << ((code[2] << 8) | code[1]); opbytes = 3; break;
        case 0xc3: out << "JMP    $" << std::setw(4) << ((code[2] << 8) | code[1]); opbytes = 3; break;
        case 0xc4: out << "CNZ    $" << std::setw(4) << ((code[2] << 8) | code[1]); opbytes = 3; break;
        case 0xc5: out << "PUSH   B"; break;
        case 0xc6: out << "ADI    #$" << std::setw(2) << (int)code[1]; opbytes = 2; break;
        case 0xc7: out << "RST    0"; break;
        case 0xc8: out << "RZ"; break;
        case 0xc9: out << "RET"; break;
        case 0xca: out << "JZ     $" << std::setw(4) << ((code[2] << 8) | code[1]); opbytes = 3; break;
        case 0xcb: out << "NOP"; break;
        case 0xcc: out << "CZ     $" << std::setw(4) << ((code[2] << 8) | code[1]); opbytes = 3; break;
        case 0xcd: out << "CALL   $" << std::setw(4) << ((code[2] << 8) | code[1]); opbytes = 3; break;
        case 0xce: out << "ACI    #$" << std::setw(2) << (int)code[1]; opbytes = 2; break;
        case 0xcf: out << "RST    1"; break;

        case 0xd0: out << "RNC"; break;
        case 0xd1: out << "POP    D"; break;
        case 0xd2: out << "JNC    $" << std::setw(4) << ((code[2] << 8) | code[1]); opbytes = 3; break;
        case 0xd3: out << "OUT    #$" << std::setw(2) << (int)code[1]; opbytes = 2; break;
        case 0xd4: out << "CNC    $" << std::setw(4) << ((code[2] << 8) | code[1]); opbytes = 3; break;
        case 0xd5: out << "PUSH   D"; break;
        case 0xd6: out << "SUI    #$" << std::setw(2) << (int)code[1]; opbytes = 2; break;
        case 0xd7: out << "RST    2"; break;
        case 0xd8: out << "RC"; break;
        case 0xd9: out << "NOP"; break;
        case 0xda: out << "JC     $" << std::setw(4) << ((code[2] << 8) | code[1]); opbytes = 3; break;
        case 0xdb: out << "IN     #$" << std::setw(2) << (int)code[1]; opbytes = 2; break;
        case 0xdc: out << "CC     $" << std::setw(4) << ((code[2] << 8) | code[1]); opbytes = 3; break;
        case 0xdd: out << "NOP"; break;
        case 0xde: out << "SBI    #$" << std::setw(2) << (int)code[1]; opbytes = 2; break;
        case 0xdf: out << "RST    3"; break;

        case 0xe0: out << "RPO"; break;
        case 0xe1: out << "POP    H"; break;
        case 0xe2: out << "JPO    $" << std::setw(4) << ((code[2] << 8) | code[1]); opbytes = 3; break;
        case 0xe3: out << "XTHL"; break;
        case 0xe4: out << "CPO    $" << std::setw(4) << ((code[2] << 8) | code[1]); opbytes = 3; break;
        case 0xe5: out << "PUSH   H"; break;
        case 0xe6: out << "ANI    #$" << std::setw(2) << (int)code[1]; opbytes = 2; break;
        case 0xe7: out << "RST    4"; break;
        case 0xe8: out << "RPE"; break;
        case 0xe9: out << "PCHL"; break;
        case 0xea: out << "JPE    $" << std::setw(4) << ((code[2] << 8) | code[1]); opbytes = 3; break;
        case 0xeb: out << "XCHG"; break;
        case 0xec: out << "CPE    $" << std::setw(4) << ((code[2] << 8) | code[1]); opbytes = 3; break;
        case 0xed: out << "NOP"; break;
        case 0xee: out << "XRI    #$" << std::setw(2) << (int)code[1]; opbytes = 2; break;
        case 0xef: out << "RST    5"; break;

        case 0xf0: out << "RP"; break;
        case 0xf1: out << "POP    PSW"; break;
        case 0xf2: out << "JP     $" << std::setw(4) << ((code[2] << 8) | code[1]); opbytes = 3; break;
        case 0xf3: out << "DI"; break;
        case 0xf4: out << "CP     $" << std::setw(4) << ((code[2] << 8) | code[1]); opbytes = 3; break;
        case 0xf5: out << "PUSH   PSW"; break;
        case 0xf6: out << "ORI    #$" << std::setw(2) << (int)code[1]; opbytes = 2; break;
        case 0xf7: out << "RST    6"; break;
        case 0xf8: out << "RM"; break;
        case 0xf9: out << "SPHL"; break;
        case 0xfa: out << "JM     $" << std::setw(4) << ((code[2] << 8) | code[1]); opbytes = 3; break;
        case 0xfb: out << "EI"; break;
        case 0xfc: out << "CM     $" << std::setw(4) << ((code[2] << 8) | code[1]); opbytes = 3; break;
        case 0xfd: out << "NOP"; break;
        case 0xfe: out << "CPI    #$" << std::setw(2) << (int)code[1]; opbytes = 2; break;
        case 0xff: out << "RST    7"; break;

        default:
            out << "UNKNOWN  (0x" << std::hex << (int)opcode << ")";
            break;
    }

    if (out_len) *out_len = opbytes;
    return out.str();
}

// Returns the number of bytes consumed by the instruction
int disassemble8080(uint8_t *codebuffer, int pc) {
    int opbytes = 1;
    std::cout << disassemble8080_str(codebuffer, pc, &opbytes) << std::dec << std::endl;
    return opbytes;
}
