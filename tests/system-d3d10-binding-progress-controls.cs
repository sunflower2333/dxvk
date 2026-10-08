// SPDX-License-Identifier: MIT
// Real tiny child / file-progress control. No registry/KMT/driver/GPU calls.
using System;
using System.Diagnostics;
using System.IO;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

public static class DxvkBindingProgressControls01 {
    static string Json(string value) { return value == null ? "null" : "\""+value.Replace("\\","\\\\").Replace("\"","\\\"").Replace("\r","\\r").Replace("\n","\\n")+"\""; }
    public static int Run(string executable,string arguments,string directory) {
        if (Directory.Exists(directory)) throw new InvalidOperationException("Fresh progress-control directory required");
        Directory.CreateDirectory(directory);
        string output=Path.Combine(directory,"child.stdout.raw"), error=Path.Combine(directory,"child.stderr.raw"), release=Path.Combine(directory,"release");
        var task=Task.Factory.StartNew(delegate { return DxvkRawProcessF4_02.Run(executable,arguments,directory,output,error,10000); });
        bool observed=false;
        var clock=Stopwatch.StartNew();
        while (!task.IsCompleted && clock.ElapsedMilliseconds < 2500) {
            if (File.Exists(output)) {
                using (var stream=new FileStream(output,FileMode.Open,FileAccess.Read,FileShare.ReadWrite))
                using (var reader=new StreamReader(stream,Encoding.UTF8))
                    observed=reader.ReadToEnd().Contains("HELD binding-progress-control");
                if (observed) break;
            }
            Thread.Sleep(10);
        }
        // Release even on a failed progress check, retain real exit/raw output.
        File.WriteAllText(release,"released-after-progress-observation="+observed,Encoding.UTF8);
        var row=task.GetAwaiter().GetResult();
        File.WriteAllText(Path.Combine(directory,"child-retained-process.json"),
            "{\"pid\":"+row.Pid+",\"retained_handle\":"+row.ProcessHandle+",\"start_utc\":"+Json(row.StartUtc)
            +",\"exited\":"+row.Exited.ToString().ToLowerInvariant()+",\"exit_code_available\":"+row.ExitCodeAvailable.ToString().ToLowerInvariant()
            +",\"exit_code\":"+row.ExitCode+",\"timed_out\":"+row.TimedOut.ToString().ToLowerInvariant()+",\"child_still_running\":"+row.ChildStillRunning.ToString().ToLowerInvariant()
            +",\"pipes_drained\":"+row.PipesDrained.ToString().ToLowerInvariant()+",\"stdout_bytes\":"+row.StdoutBytes+",\"stderr_bytes\":"+row.StderrBytes
            +",\"capture_failure\":"+Json(row.Failure)+",\"observed_before_release\":"+observed.ToString().ToLowerInvariant()
            +",\"executable\":"+Json(executable)+",\"arguments\":"+Json(arguments)+",\"hardware_admission\":false,\"registration\":false}",Encoding.UTF8);
        bool passed=observed && row.Pid>0 && row.ProcessHandle!=0 && row.Exited && row.ExitCodeAvailable && row.ExitCode==0 && !row.TimedOut && !row.ChildStillRunning && row.PipesDrained && row.Failure==null && row.StderrBytes==0;
        Console.WriteLine("D10 binding progress controls "+(passed ? "PASS" : "FAIL")+" observed_before_release="+(observed ? "1" : "0")+" child_exit="+row.ExitCode+" hardware_admission=0");
        return passed ? 0 : 1;
    }
}
