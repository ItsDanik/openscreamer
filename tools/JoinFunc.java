// Join a routine the analysis cut in two: remove the function that starts at each BODY address
// and re-make the one at its ENTRY, so that the body is part of the entry's routine again.
// The menu's pages begin with a short prologue (the frame set up) and fall through into the
// rest; something else pointed into the rest -- even into the middle of an instruction -- and
// a second function was made there, which then decompiles with `unaff_EBP` (the frame it never
// set up).  The listing from the entry to the end of the false function is cleared and
// disassembled again from the entry.
//   analyzeHeadless ... -postScript JoinFunc.java <entry> <body> <size> [...]   (size: of the false
//   function, used when it is gone already)
import ghidra.app.script.GhidraScript;
import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.Function;

public class JoinFunc extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] a = getScriptArgs();
        for (int i = 0; i + 2 < a.length; i += 3) {
            Address entry = toAddr(Long.decode(a[i]));
            Address body = toAddr(Long.decode(a[i + 1]));
            Address end = body.add(Long.decode(a[i + 2]) + 0x10);
            Function fb = getFunctionAt(body);
            if (fb != null) { end = fb.getBody().getMaxAddress(); removeFunction(fb); }
            Function fe = getFunctionAt(entry);
            if (fe != null) removeFunction(fe);
            clearListing(entry, end);
            DisassembleCommand dis = new DisassembleCommand(new AddressSet(entry, end), null, true);
            dis.applyTo(currentProgram, monitor);
            CreateFunctionCmd cmd = new CreateFunctionCmd(entry);
            boolean ok = cmd.applyTo(currentProgram, monitor);
            Function f = getFunctionAt(entry);
            println("JOIN: " + a[i] + " + " + a[i + 1] + " -> " + (ok && f != null ? f.getBody().getNumAddresses() + " bytes" : "FAILED"));
        }
    }
}
