//Create functions at the addresses listed in a file (one hex address a line).
//usage: -postScript SeedFuncs.java build/ghidra2/seeds.txt
//The list comes from the LE fixup records (tools/unle.py --fixups): every pointer into the
//code object that is not a jump-table entry is a function somebody takes the address of.
//@category Screamer

import ghidra.app.script.GhidraScript;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.program.model.address.Address;
import java.nio.file.Files;
import java.nio.file.Paths;

public class SeedFuncs extends GhidraScript {
    @Override
    public void run() throws Exception {
        int made = 0, had = 0, bad = 0;
        for (String line : Files.readAllLines(Paths.get(getScriptArgs()[0]))) {
            line = line.trim();
            if (line.isEmpty()) continue;
            Address a = toAddr(Long.decode(line));
            if (currentProgram.getFunctionManager().getFunctionAt(a) != null) { had++; continue; }
            // a pointer into the middle of something already disassembled is a label, not a function
            if (currentProgram.getListing().getInstructionContaining(a) != null
                    && currentProgram.getListing().getInstructionAt(a) == null) { bad++; continue; }
            new DisassembleCommand(a, null, true).applyTo(currentProgram, monitor);
            if (new CreateFunctionCmd(a).applyTo(currentProgram, monitor)) made++; else bad++;
        }
        println("SEEDS: " + made + " functions created, " + had + " already there, " + bad + " refused");
    }
}
