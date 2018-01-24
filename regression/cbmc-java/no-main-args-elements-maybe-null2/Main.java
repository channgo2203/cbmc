public class Main
{
  static void main(String[] args) // not public!
  {
    if(args != null && args.length>0 && args[0] != null) {
      assert(false); // allowed to fail
    }
  }
}

