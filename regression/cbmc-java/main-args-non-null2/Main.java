public class Main
{
  public static void main(String[] args)
  {
    try {
      int i = args.length; // args must be non-null
    }
    catch(Exception e) {
      assert false; // unreachable
    }
  }
}
