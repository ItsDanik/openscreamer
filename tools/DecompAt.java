//Decompile functions at exact entry addresses, creating them if analysis never reached them.
//usage: -postScript DecompAt.java 0x28080 0x29530 ...
//@category Screamer
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.util.task.ConsoleTaskMonitor;

public class DecompAt extends GhidraScript {
    @Override
    public void run() throws Exception {
        DecompInterface di = new DecompInterface();
        di.openProgram(currentProgram);
        ConsoleTaskMonitor mon = new ConsoleTaskMonitor();
        // 0x4d7b4 is Watcom's float->int rounding helper.  Ghidra marks it "does not
        // return", which silently truncates every function that converts a float.
        Function rnd = currentProgram.getFunctionManager().getFunctionAt(toAddr(0x4d7b4L));
        if (rnd != null && rnd.hasNoReturn()) rnd.setNoReturn(false);
        for (String arg : getScriptArgs()) {
            Address ea = toAddr(Long.parseLong(arg.replace("0x", ""), 16));
            Function f = currentProgram.getFunctionManager().getFunctionAt(ea);
            if (f == null) {
                new DisassembleCommand(ea, null, true).applyTo(currentProgram, mon);
                new CreateFunctionCmd(ea).applyTo(currentProgram, mon);
                f = currentProgram.getFunctionManager().getFunctionAt(ea);
            }
            if (f == null) { println("NOFUNC " + ea); continue; }
            DecompileResults res = di.decompileFunction(f, 240, mon);
            println("=====DECOMP " + ea + " size=" + f.getBody().getNumAddresses() + "=====");
            if (res.decompileCompleted()) println(res.getDecompiledFunction().getC());
            else println("FAILED " + res.getErrorMessage());
        }
    }
}
