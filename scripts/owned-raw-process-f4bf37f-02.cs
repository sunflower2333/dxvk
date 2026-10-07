using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Threading.Tasks;

public sealed class DxvkRawProcessResultF4_02 {
    public int Pid;
    public string StartUtc;
    public long ProcessHandle;
    public bool Exited;
    public bool ExitCodeAvailable;
    public int ExitCode;
    public bool TimedOut;
    public bool ChildStillRunning;
    public bool PipesDrained;
    public long StdoutBytes;
    public long StderrBytes;
    public double Seconds;
    public string Failure;
}

public static class DxvkRawProcessF4_02 {
    private static void Fail(DxvkRawProcessResultF4_02 row, string failure) {
        row.Failure = String.IsNullOrEmpty(row.Failure) ? failure : row.Failure + "\n" + failure;
    }

    // One Process object owns the original OS handle until ExitCode and both
    // raw output streams have been captured. No Start-Process or Refresh.
    public static DxvkRawProcessResultF4_02 Run(string executable, string arguments,
            string directory, string stdout, string stderr, int deadlineMs) {
        if (deadlineMs <= 0) throw new ArgumentOutOfRangeException("deadlineMs");
        var row = new DxvkRawProcessResultF4_02();
        var timer = Stopwatch.StartNew();
        var process = new Process();
        FileStream output = null, error = null;
        var copies = new List<Task>();
        bool started = false;
        try {
            output = new FileStream(stdout, FileMode.CreateNew, FileAccess.Write, FileShare.Read);
            error = new FileStream(stderr, FileMode.CreateNew, FileAccess.Write, FileShare.Read);
            process.StartInfo.FileName = executable;
            process.StartInfo.Arguments = arguments;
            process.StartInfo.WorkingDirectory = directory;
            process.StartInfo.UseShellExecute = false;
            process.StartInfo.CreateNoWindow = true;
            process.StartInfo.RedirectStandardOutput = true;
            process.StartInfo.RedirectStandardError = true;
            if (!process.Start()) throw new InvalidOperationException("Owned child did not start");
            started = true;
            row.Pid = process.Id;
            row.StartUtc = process.StartTime.ToUniversalTime().ToString("o");
            row.ProcessHandle = process.Handle.ToInt64();
            copies.Add(process.StandardOutput.BaseStream.CopyToAsync(output, 65536));
            copies.Add(process.StandardError.BaseStream.CopyToAsync(error, 65536));
            row.TimedOut = !process.WaitForExit(deadlineMs);
        } catch (Exception exception) {
            Fail(row, exception.ToString());
        } finally {
            if (started) {
                try {
                    if (!process.HasExited) {
                        try { process.Kill(); }
                        catch { if (!process.HasExited) throw; }
                        if (!process.WaitForExit(5000)) {
                            row.ChildStillRunning = true;
                            Fail(row, "Owned child did not exit after Kill deadline");
                        }
                    }
                    row.Exited = process.HasExited;
                    if (row.Exited) {
                        row.ExitCode = process.ExitCode;
                        row.ExitCodeAvailable = true;
                    }
                } catch (Exception exception) {
                    row.ChildStillRunning = true;
                    Fail(row, exception.ToString());
                }
            }
            try {
                row.PipesDrained = started && copies.Count == 2 && Task.WaitAll(copies.ToArray(), 20000);
                if (started && !row.PipesDrained) Fail(row, "Owned raw pipe drain failed or exceeded deadline");
            } catch (Exception exception) {
                Fail(row, exception.ToString());
            }
            try { if (output != null) { output.Flush(); row.StdoutBytes = output.Length; } }
            catch (Exception exception) { Fail(row, exception.ToString()); }
            try { if (error != null) { error.Flush(); row.StderrBytes = error.Length; } }
            catch (Exception exception) { Fail(row, exception.ToString()); }
            try { if (output != null) output.Dispose(); }
            catch (Exception exception) { Fail(row, exception.ToString()); }
            try { if (error != null) error.Dispose(); }
            catch (Exception exception) { Fail(row, exception.ToString()); }
            try { process.Dispose(); }
            catch (Exception exception) { Fail(row, exception.ToString()); }
            timer.Stop();
            row.Seconds = timer.Elapsed.TotalSeconds;
        }
        return row;
    }
}
