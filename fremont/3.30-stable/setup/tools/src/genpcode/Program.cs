using System.Xml;
using System;

class Program
{
    public static void AddProductCode(XmlNode pnode, string version, string platform)
    {
        string xpath = string.Format("./product-code[@version='{0}' and @platform='{1}']", version, platform);
        XmlNode n = pnode.SelectSingleNode(xpath);
        string pcode;
        string remark;
        if (null != n)
        {
            pcode = n.InnerText;
            remark = " (existing)";
        }
        else
        {
            pcode = Guid.NewGuid().ToString("D").ToUpper();
            XmlElement e = pnode.OwnerDocument.CreateElement("product-code");
            remark = " (new)";
            e.InnerText = pcode;
            e.SetAttribute("version", version);
            e.SetAttribute("platform", platform);
            pnode.AppendChild(e);
        }
        Console.WriteLine("{1}-{2,-6}: {0}{3}", pcode, version, platform, remark);
    }

    public static int Main(string[] args)
    {
        if (args.Length < 2)
        {
            Console.Error.WriteLine("usage: getpcode <product-code-xml-file> <version>");
            return -1;
        }

        string filename = args[0];
        string version = args[1];

        XmlDocument xmldoc = new XmlDocument();
        xmldoc.Load(filename);
        XmlNode pnode = xmldoc.SelectSingleNode("/setup/product-codes");

        AddProductCode(pnode, version, "i386");
        AddProductCode(pnode, version, "amd64");

        xmldoc.Save(filename);

        return 0;
    }
}
