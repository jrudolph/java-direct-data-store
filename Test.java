import java.util.Arrays;

public class Test {
  static {
    System.loadLibrary("test");
  }

  public static native Person persist(Person p);
  public static native Person load(Class<Person> cl);
  public static native <T> T analyze(T t);
  
  public static void pers() {
    Person p = new Person();
    p.age = 12;
    Person copy = persist(p);
    System.out.println(copy.age+" cl: "+copy.getClass());
    System.gc();
    System.out.println(copy.age);
  }
  
  public static void load() {
    Person copy = load(Person.class);
    System.out.println(copy.age);
    //System.out.println(copy.getClass().getSimpleName());
    //System.out.println(copy.age+" cl: "+copy.getClass());
    //System.gc();
    System.out.println(copy.age+" name: "+copy.name);
    for(Person p: copy.parents)
      System.out.println(p.name+" "+p.age);
  }
  
  public static void analyze() {
    /*Person mom = new Person();
    mom.age = 52;
    mom.name = "Mom";
    
    Person p = new Person();
    p.age = 26;
    p.name = "Peter";
    p.parents = Arrays.asList(mom, mom, mom);
    Person copy = analyze(p);
    
    System.out.println(copy.age);
    System.out.println(copy.age+" cl: "+copy.getClass());
    System.gc();
    System.out.println(copy.age+" name: "+copy.name);
    for(Person p2: copy.parents)
      System.out.println(p2.name+" "+p2.age);*/
      
    String test = "wurst";
    String newS = analyze(test);
    System.out.println(newS);
    System.out.println(newS.getClass());
    System.gc();
    System.out.println(newS);
  }
  public static void main(String[] args) throws InterruptedException {
    //Thread.sleep(20000);
    //pers();
    //load();
    analyze();
  }
}
