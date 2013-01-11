using System;
using System.Collections;
using System.Collections.Generic;
using System.Text;
using System.IO;
using System.Diagnostics;
using System.Runtime.InteropServices;

class Program
{
    [DllImport("kernel32.dll", CharSet = CharSet.Auto)]
    public static extern bool CreateHardLink(
        string FileName, 
        string ExistingFileName, 
        IntPtr Reserved);

    void DisplayUsage()
    {
        BuildMsg("usage: bldpub.exe -F <publish-file>");
    }

    bool ProcessLine(string line)
    {
        // {src=dest1;dest2;...}
        // { } is optional

        line = line.Trim();
        line = line.TrimStart('{');
        line = line.TrimEnd('}');
        line = line.Trim();

        if (0 == line.Length)
        {
            // ignore the blank line
            return true;
        }

        string[] sdpair = line.Split('=');
        if (sdpair.Length < 2)
        {
            BuildMsg("Invalid line - {0}", line);
            return false;
        }

        string srcfile = sdpair[0];
        string[] destfiles = sdpair[1].Split(';');

        foreach (string destfile in destfiles)
        {
            BuildMsg("{0} -> {1}", srcfile, destfile);

            try
            {
                string destdir = Path.GetDirectoryName(destfile);

                if (!Directory.Exists(destdir))
                {
                    Directory.CreateDirectory(destdir);
                }
            }
            catch (Exception ex)
            {
                BuildMsg("(CreateDirectory) - {0}", ex);
            }

            if (File.Exists(destfile))
            {
                try
                {
                    File.SetAttributes(destfile, FileAttributes.Normal);
                    File.Delete(destfile);
                }
                catch (Exception ex)
                {
                    BuildMsg("(Delete) - {0}", ex);
                }
            }
#if _USE_HARDLINK_
            if (!CreateHardLink(destfile, srcfile, IntPtr.Zero))
            {
                Console.WriteLine("BUILDMSG: bldpub: CreateHardLink failed.");
            }
#else
            try
            {
                File.Copy(srcfile, destfile, true);
            }
            catch (Exception ex)
            {
                BuildMsg("(Copy) - {0}", ex);
            }

            try
            {
                DateTime srcdt = File.GetLastWriteTime(srcfile);
                DateTime destdt = File.GetLastWriteTime(destfile);

                if (srcdt != destdt)
                {
                    try
                    {
                        File.SetLastWriteTime(destfile, srcdt);
                    }
                    catch (Exception ex)
                    {
                        BuildMsg("(SetLastWriteTime) - {0}", ex);
                    }
                }
            }
            catch (Exception ex)
            {
                BuildMsg("(GetLastWriteTime) - {0}", ex);
            }

            if ("PASS0".Equals(_BuildPass, StringComparison.InvariantCultureIgnoreCase))
            {
                try
                {
                    File.SetAttributes(destfile, FileAttributes.ReadOnly);
                }
                catch (Exception ex)
                {
                    BuildMsg("(SetAttributes) - {0}", ex);
                }
            }

#endif
        }

        return true;
    }

    void BuildMsg(string format, params object[] arg)
    {
        Console.WriteLine("BUILDMSG:{0}{1}", _BuildMsg, string.Format(format, arg));
    }

    string _BuildPass;
    string _BuildMsg;

    int Run(string[] args)
    {
        _BuildPass = Environment.GetEnvironmentVariable("BUILD_PASS");
        _BuildMsg = Environment.GetEnvironmentVariable("BUILDMSG");

        if (!string.IsNullOrEmpty(_BuildMsg))
        {
            _BuildMsg += " ";
        }

        if (args.Length < 1)
        {
            Console.WriteLine("BUILDMSG: No arguments are specified.");
            DisplayUsage();
            return 1;
        }

        string inputfile = null;
        for (int i = 0; i < args.Length; ++i)
        {
            if (string.Equals(args[i], "-f", StringComparison.OrdinalIgnoreCase) ||
                string.Equals(args[i], "/f", StringComparison.OrdinalIgnoreCase))
            {
                if (i + 1 < args.Length)
                {
                    inputfile = args[i + 1];
                    break;
                }
                else
                {
                    BuildMsg("File name is not specified after -f");
                    DisplayUsage();
                    return 1;
                }
            }
        }

        if (null == inputfile)
        {
            BuildMsg("No input file is specified");
            DisplayUsage();
            return 1;
        }

        try
        {
            using (StreamReader reader = File.OpenText(inputfile))
            {
                string line;
                while (null != (line = reader.ReadLine()))
                {
                    ProcessLine(line);
                }
                reader.Close();
            }
        }
        catch (Exception ex)
        {
            BuildMsg("{0}", ex);
            return 1;
        }

        return 0;
    }
    static int Main(string[] args)
    {
#if ENVDEBUG
        foreach (DictionaryEntry de in Environment.GetEnvironmentVariables(EnvironmentVariableTarget.Process))
        {
            Console.WriteLine("BUILDMSG: " + de.Key.ToString() + "=" + de.Value.ToString());
        }
#endif
        return new Program().Run(args);
    }
}
