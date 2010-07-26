public class Test {
  static {
    System.loadLibrary("test");
  }

  public static native Person get();
  
  public static void main(String[] args) {
    System.out.println(get().age);
  }
}
