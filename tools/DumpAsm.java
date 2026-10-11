//Dump the assembly of a function (or an address range).
//usage: -postScript DumpAsm.java 0x4d284
//@category Screamer

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionManager;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.listing.Listing;

public class DumpAsm extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 1) { println("usage: DumpAsm.java <hexaddr>"); return; }
        long va = Long.parseLong(args[0].replace("0x", ""), 16);
        Address ea = toAddr(va);

        FunctionManager fm = currentProgram.getFunctionManager();
        Function f = fm.getFunctionAt(ea);
        if (f == null) f = fm.getFunctionContaining(ea);
        if (f == null) { println("no function at " + ea); return; }

        println("ASSEMBLY of " + f.getName() + " @ " + f.getEntryPoint()
                + "  size=" + f.getBody().getNumAddresses());
        Listing listing = currentProgram.getListing();
        InstructionIterator it = listing.getInstructions(f.getBody(), true);
        while (it.hasNext()) {
            Instruction ins = it.next();
            println(String.format("%s  %-30s %s", ins.getAddress(),
                    ins.toString(), ""));
        }
    }
}
