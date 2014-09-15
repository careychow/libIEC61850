using System;
using System.Collections.Generic;

using IEC61850.Client;
using IEC61850.Common;

namespace datasets
{
    class DataSetExample
    {
        public static void Main (string[] args)
        {
            IedConnection con = new IedConnection ();

            string hostname;

            if (args.Length > 0)
                hostname = args[0];
            else
                hostname = "10.0.2.2";

            Console.WriteLine("Connect to " + hostname);

            try
            {
                con.Connect(hostname, 102);

                List<string> serverDirectory = con.GetServerDirectory(false);

                foreach (string entry in serverDirectory)
                {
                    Console.WriteLine("LD: " + entry);
                }


				// create a new data set

				List<string> dataSetElements = new List<string>();

				dataSetElements.Add("IEDM1CPUBHKW/DRCS1.ModOnConn.stVal[ST]");
				dataSetElements.Add("IEDM1CPUBHKW/DRCS1.ModOnConn.t[ST]");

				con.CreateDataSet("IEDM1CPUBHKW/LLN0.ds1", dataSetElements);

				// get the directory of the data set
				List<string> dataSetDirectory = con.GetDataSetDirectory("IEDM1CPUBHKW/LLN0.ds1");

				foreach (string entry in dataSetDirectory)
                {
                    Console.WriteLine("DS element: " + entry);
                }


				// read the values of the newly created data set
                DataSet dataSet = con.ReadDataSetValues("IEDM1CPUBHKW/LLN0.ds1", null);

				MmsValue dataSetValues = dataSet.GetValues();

				Console.WriteLine ("Data set contains " + dataSetValues.Size() + " elements");

				foreach (MmsValue value in dataSetValues) {
					Console.WriteLine("  DS value: " + value + " type: " + value.GetType());
				}


				// delete the data set
				con.DeleteDataSet("IEDM1CPUBHKW/LLN0.ds1");

                con.Abort();
            }
            catch (IedConnectionException e)
            {
				Console.WriteLine(e.Message);
           }

        }
    }
}
