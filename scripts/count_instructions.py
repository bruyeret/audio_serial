import re

instruction_cycles = {
    "ADD": 1,
    "ADC": 1,
    "ADIW": 2,
    "SUB": 1,
    "SUBI": 1,
    "SBC": 1,
    "SBCI": 1,
    "SBIW": 2,
    "AND": 1,
    "ANDI": 1,
    "OR": 1,
    "ORI": 1,
    "EOR": 1,
    "COM": 1,
    "NEG": 1,
    "SBR": 1,
    "CBR": 1,
    "INC": 1,
    "DEC": 1,
    "TST": 1,
    "CLR": 1,
    "SER": 1,
    "MUL": 2,
    "MULS": 2,
    "MULSU": 2,
    "FMUL": 2,
    "FMULS": 2,
    "FMULSU": 2,
    "SBI": 2,
    "CBI": 2,
    "LSL": 1,
    "LSR": 1,
    "ROL": 1,
    "ROR": 1,
    "ASR": 1,
    "SWAP": 1,
    "BSET": 1,
    "BCLR": 1,
    "BST": 1,
    "BLD": 1,
    "SEC": 1,
    "CLC": 1,
    "SEN": 1,
    "CLN": 1,
    "SEZ": 1,
    "CLZ": 1,
    "SEI": 1,
    "CLI": 1,
    "SES": 1,
    "CLS": 1,
    "SEV": 1,
    "CLV": 1,
    "SET": 1,
    "CLT": 1,
    "SEH": 1,
    "CLH": 1,
    "MOV": 1,
    "MOVW": 1,
    "LDI": 1,
    "LD": 2,
    "LD": 2,
    "LD": 2,
    "LD": 2,
    "LD": 2,
    "LD": 2,
    "LDD": 2,
    "LD": 2,
    "LD": 2,
    "LD": 2,
    "LDD": 2,
    "LDS": 2,
    "ST": 2,
    "ST": 2,
    "ST": 2,
    "ST": 2,
    "ST": 2,
    "ST": 2,
    "STD": 2,
    "ST": 2,
    "ST": 2,
    "ST": 2,
    "STD": 2,
    "STS": 2,
    "LPM": 3,
    "LPM": 3,
    "LPM": 3,
    "IN": 1,
    "OUT": 1,
    "PUSH": 2,
    "POP": 2,
    "RET": 4,
    "RETI": 4,
    "CP": 1,
    "CPC": 1,
    "CPI": 1,
}

def main():
    # # FFT
    # last_instruction = "RET"
    # function_name_pattern = ".*approx_fft.*"

    # ADC conversion done interrupt
    last_instruction = "RETI"
    function_name_pattern = "__vector_21"
    # In this function, the branch instruction is an early return
    instruction_cycles["BRCC"] = 1

    with open("dissassembly.asm", "r") as file:
        # Find the beginning of the function
        function_pattern = f"[a-f0-9]* <{function_name_pattern}>:"
        function_prog = re.compile(function_pattern)
        for line in file:
            if function_prog.match(line) is not None:
                print(line[:-1])
                break
        
        # Get the number of occurence for each instructions
        instruction_pattern = "\t.*\t([a-z]*)(\t|$)"
        instruction_prog = re.compile(instruction_pattern)
        instruction_counts = dict()
        for line in file:
            match = instruction_prog.search(line)
            if match is None:
                raise AssertionError(f"No matched instruction in function for line:\n{line}")
            instruction = match.group(1).upper()
            new_count = instruction_counts.get(instruction, 0) + 1
            instruction_counts[instruction] = new_count
            if instruction == last_instruction:
                break

    # Count total cycles and instructions
    total_cycles = 0
    total_instructions = 0
    for instruction, count in instruction_counts.items():
        cycles = instruction_cycles.get(instruction)
        if cycles is None:
            raise AssertionError(f"Unknown instruction weight for {instruction}")
        total_cycles += count * cycles
        total_instructions += count

    print(f"Cycles: {total_cycles}")
    print(f"Instructions: {total_instructions}")
    print("Summary of instructions:")
    percentages = dict()
    for instruction, count in instruction_counts.items():
        cycles = instruction_cycles.get(instruction)
        percentage_cycles = 100 * count * cycles / total_cycles
        percentages[instruction] = percentage_cycles
    for instruction, percentage_cycles in sorted(percentages.items(), key=lambda item: -item[1]):
        print(f"{instruction}\t takes \t{percentage_cycles:.2f}% of cycles")

if __name__ == "__main__":
    main()
