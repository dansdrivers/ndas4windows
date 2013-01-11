using System;
using System.IO;
using System.Security.Cryptography;

public class HashGen
{
	[STAThread]
	public static void Main(string[] args)
	{
		foreach (string filePath in args)
		{
            try
            {
                FileStream fs = new FileStream(filePath, FileMode.Open, FileAccess.Read);
                // This is one implementation of the abstract class MD5.
                MD5 md5 = new MD5CryptoServiceProvider();
                byte[] result = md5.ComputeHash(fs);
                fs.Close();
                string value = BitConverter.ToString(result).Replace("-", "");
                Console.WriteLine("{0}", value);
            }
            catch (Exception ex)
            {
                Console.Write(ex);
            }
		}
	}
}
