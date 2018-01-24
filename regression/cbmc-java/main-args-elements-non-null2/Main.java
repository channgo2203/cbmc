public class Main
{
  public static void main(String[] args)
  {
    try {
      if(args.length>0)
      {
	String s = args[0];
	int i = s.length(); // must not fail
      }
    }
    catch(Exception e) {
      assert false; // unreachable
    }
  }
}

