# [INTEGRATION] COPIED FROM: Cornerstone-Project-col7001-lab-4-and-5
import sys
import struct

# Dictionary mapping instruction names to their corresponding opcode values
# These hex values correspond to the specific instructions for our VM
OPCODES = {
    "PUSH": 0x01, "POP": 0x02, "DUP": 0x03, "HALT": 0xFF,
    "ADD": 0x10, "SUB": 0x11, "MUL": 0x12, "DIV": 0x13, "CMP": 0x14,
    "JMP": 0x20, "JZ": 0x21, "JNZ": 0x22,
    "STORE": 0x30, "LOAD": 0x31, "CALL": 0x40, "RET": 0x41,
    "PRINT": 0x50, "INPUT": 0x51, "ALLOC": 0x60
}

def assemble(input_file, output_file):
    """
    Reads an assembly source file and converts it into binary bytecode.
    It uses a two-pass approach to handle labels and forward jumps.
    """
    
    # Read the entire input file into a list of lines
    with open(input_file, 'r') as f:
        lines = f.readlines()

    # --- Pass 1: Find Labels ---
    # We scan the code first to find where all the labels (like 'LOOP:') are located.
    # This allows us to know the address of a label even if we jump to it before it's defined.
    labels = {}
    addr = 0 # Keeps track of the current byte address in the binary
    
    for line in lines:
        # Clean up the line: remove comments (starting with ';') and split into words
        parts = line.split(';')[0].split()
        if not parts: continue # Skip empty lines
        
        # Check if the first word is a label definition (ends with ':')
        if parts[0].endswith(':'):
            label_name = parts[0][:-1] # Remove the colon
            labels[label_name] = addr  # Store the current address for this label
            continue # Labels themselves don't take up space in the binary, so we move to next line

        # If it's a valid instruction, advance the address counter
        instr = parts[0].upper()
        if instr in OPCODES:
            addr += 1 # The opcode itself takes 1 byte
            if len(parts) > 1: 
                addr += 4 # The argument (if present) takes 4 bytes (integer)

    # --- Pass 2: Generate Bytecode ---
    # Now we scan the code a second time to actually generate the binary data.
    bytecode = bytearray()
    
    for line in lines:
        parts = line.split(';')[0].split()
        if not parts: continue
        
        # We already handled labels in Pass 1, so we just skip them here
        if parts[0].endswith(':'):
            continue
        
        instr = parts[0].upper()
        if instr in OPCODES:
            # 1. Write the Opcode
            bytecode.append(OPCODES[instr])
            
            # 2. Write the Argument (if the instruction has one)
            if len(parts) > 1:
                arg = parts[1]
                
                # Check if the argument is a label name
                if arg in labels:
                    val = labels[arg] # Replace label with its calculated address
                else:
                    val = int(arg)    # Otherwise, treat it as a regular number
                
                # Pack the value as a 32-bit little-endian integer
                bytecode.extend(struct.pack("<i", val))
        
    # Write the final sequence of bytes to the output file
    with open(output_file, 'wb') as f:
        f.write(bytecode)

if __name__ == "__main__":
    # Ensure the user provides input and output filenames
    if len(sys.argv) < 3:
        print("Usage: python3 assembler.py <input.asm> <output.bin>")
    else:
        assemble(sys.argv[1], sys.argv[2])
