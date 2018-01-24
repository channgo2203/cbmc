public class Main
{
  public static void main(String[] args)
  {
    try {
      if(args.length>0)
      {
	String s = args[0];
	if(s != null) {
	  int i = s.length(); // definitely must not fail
	}
      }
    }
    catch(Exception e) {
      assert false; // unreachable
    }
  }
}

