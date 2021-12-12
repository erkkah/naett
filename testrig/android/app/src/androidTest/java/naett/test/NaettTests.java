package naett.test;

import org.junit.Test;
import static org.junit.Assert.assertTrue;

public class NaettTests {
    @Test
    public void naett_Test() {
        System.loadLibrary("native-activity");
        assertTrue(runTests() != 0);
    }

    private native int runTests();
}
