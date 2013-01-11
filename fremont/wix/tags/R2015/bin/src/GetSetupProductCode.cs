using System;
using System.Xml;

public class SetupProductCode
{
	public static int Usage()
	{
		Console.WriteLine("usage: GetSetupProductCode <xml-file> <version>");
		return 1;
	}

	public static int Main(string[] args)
	{
		if (2 != args.Length)
		{
			return Usage();
		}

		string xmlFile = args[0];
		string productVersion = args[1];

		try
		{	
			XmlDocument xml = new XmlDocument();
			xml.Load(xmlFile);
			string xpath = string.Format(
				"/setup/product-codes/product-code[@version='{0}']/text()", 
				productVersion);
			XmlNode node = xml.SelectSingleNode(xpath);
			if (null == node)
			{
				throw new Exception("Product code not found.");
			}

			Guid guid = new Guid(node.Value);
			string productCode = guid.ToString("D").ToUpper();

			Console.WriteLine(productCode);
			
			return 0;
		}
		catch (Exception ex)
		{
			Console.WriteLine(ex);
			return 1;
		}
	}
}
