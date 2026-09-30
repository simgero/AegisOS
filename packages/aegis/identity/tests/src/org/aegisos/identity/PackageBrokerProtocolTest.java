package org.aegisos.identity;

import static org.junit.Assert.*;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.util.Arrays;
import org.junit.Test;
import org.junit.runner.RunWith;

/** Inert frames; no users, keys, native resources, package installation or approval. */
@RunWith(AndroidJUnit4.class)
public final class PackageBrokerProtocolTest {
    private static final String HASH="aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    private static void denied(Runnable r) {
        try { r.run();fail("Malformed package frame accepted"); } catch (IllegalArgumentException expected) { }
    }
    private static PackageBrokerProtocol.Intent intent() { return new PackageBrokerProtocol.Intent(1,2,"bash","5.2"); }
    private static String common() {
        return "\"action\":1,\"scope\":2,\"package\":\"bash\",\"version\":\"5.2\",\"digest\":\""+HASH+"\",\"validUntil\":2000";
    }
    private static String change() {
        return "{\"name\":\"bash\",\"architecture\":\"arm64\",\"before\":\"5.1\",\"after\":\"5.2\",\"reason\":1}";
    }
    private static String review() { return "{"+common()+",\"changes\":["+change()+"]}"; }
    private static String status() {
        return "{"+common()+",\"phase\":6,\"outcome\":0,\"waitStatus\":0,\"error\":0,\"generation\":{\"image\":\"\",\"sharedBase\":\"\",\"bytes\":0}}";
    }
    private static byte[] frame(int op,int error,long job,int kind,String body) {
        byte[] data=body.getBytes(StandardCharsets.UTF_8);
        return ByteBuffer.allocate(48+data.length).order(ByteOrder.LITTLE_ENDIAN)
                .putInt(RuntimeBrokerProtocol.MAGIC).putShort((short)3).putShort((short)op).putLong(2)
                .putInt(10).putInt(1234).putInt(error).putInt(error==0?1:2)
                .putLong(job).putInt(kind).putInt(data.length).put(data).array();
    }
    private static PackageBrokerProtocol.Reply parse(byte[] b,int op,long job) {
        return PackageBrokerProtocol.reply(b,b.length,op,2,10,1234,job,0);
    }
    private static PackageBrokerProtocol.Metadata review(String json) { return parse(frame(10,0,44,2,json),10,44).metadata; }
    private static PackageBrokerProtocol.Metadata status(String json) { return parse(frame(12,0,44,1,json),12,44).metadata; }

    @Test public void beginGoldenMatchesNativeFrameExactly() {
        byte[] golden={0x41,0x47,0x52,0x42,3,0,8,0,2,0,0,0,0,0,0,0,
                0x64,(byte)0xca,(byte)0x9a,0x3b,0,0,0,0,10,0,0,0,(byte)0xd2,4,0,0,
                1,0,0,0,2,0,0,0,4,0,0,0,3,0,0,0,'b','a','s','h','5','.','2'};
        assertArrayEquals(golden,PackageBrokerProtocol.request(8,2,1_000_000_100L,10,1234,100,
                PackageBrokerProtocol.payload(8,0,intent(),null)));
    }
    @Test public void invalidCliSyntaxFailsBeforeConnectionOwnershipChanges() {
        for(String name:new String[]{null,"", "x", "../bash", "bash;id", "bash\n", "bash+", "bash-", "Bash", "bash:arm64"})
            denied(()->new PackageBrokerProtocol.Intent(1,1,name,""));
        for(String v:new String[]{null,"x1","1-","1:2:3","1/2","1\u0000","1 2"})
            denied(()->new PackageBrokerProtocol.Intent(1,1,"bash",v));
        new PackageBrokerProtocol.Intent(1,1,"bash","1:5.2~rc1-2");
        new PackageBrokerProtocol.Intent(2,2,"","");
        new PackageBrokerProtocol.Intent(3,1,"bash","");
        denied(()->new PackageBrokerProtocol.Intent(2,2,"bash",""));
        denied(()->new PackageBrokerProtocol.Intent(3,2,"bash","5.2"));
        denied(()->new PackageBrokerProtocol.Intent(1,0,"bash",""));
        denied(()->new PackageBrokerProtocol.Intent(4,1,"bash",""));
    }
    @Test public void continuationPayloadCannotSelectAnotherIntentOrInventDigest() {
        for(int op=9;op<=14;op++) {
            final int operation=op;String digest=op==13?HASH:null;
            byte[] payload=PackageBrokerProtocol.payload(op,44,null,digest);
            assertEquals(44,ByteBuffer.wrap(payload).order(ByteOrder.LITTLE_ENDIAN).getLong());
            assertEquals(op==13?72:8,payload.length);
            denied(()->PackageBrokerProtocol.payload(operation,0,null,digest));
            denied(()->PackageBrokerProtocol.payload(operation,44,intent(),digest));
        }
        denied(()->PackageBrokerProtocol.payload(13,44,null,null));
        denied(()->PackageBrokerProtocol.payload(13,44,null,HASH.toUpperCase()));
        denied(()->PackageBrokerProtocol.payload(14,44,null,HASH));
        denied(()->PackageBrokerProtocol.payload(8,44,intent(),null));
    }
    @Test public void nativeFailurePreservesRegisteredBeginAndCancellationJob() {
        PackageBrokerProtocol.Reply failed=parse(frame(8,5,44,0,""),8,0);
        assertEquals(5,failed.error);assertEquals(44,failed.job);assertNull(failed.metadata);
        assertEquals(0,parse(frame(8,22,0,0,""),8,0).job);
        assertEquals(44,parse(frame(14,16,44,0,""),14,44).job);
        denied(()->parse(frame(8,0,0,0,""),8,0));
        denied(()->parse(frame(14,16,0,0,""),14,44));
        denied(()->parse(frame(14,16,45,0,""),14,44));
        denied(()->parse(frame(12,5,44,1,status()),12,44));
    }
    @Test public void responsesBindEveryHeaderAndRejectDescriptorsOrTruncation() {
        byte[] valid=frame(12,0,44,1,status());
        for(int offset:new int[]{0,4,6,8,16,20,28,32,40,44}) {
            byte[] bad=valid.clone();bad[offset]^=1;denied(()->parse(bad,12,44));
        }
        for(int length:new int[]{-1,0,47,valid.length-1,valid.length+1,65537})
            denied(()->PackageBrokerProtocol.reply(valid,length,12,2,10,1234,44,0));
        for(int count:new int[]{-1,1,2})denied(()->PackageBrokerProtocol.reply(valid,valid.length,12,2,10,1234,44,count));
    }
    @Test public void reviewPreservesAllEffectsAndTheirAutomaticDependencyMarks() {
        String two=review().replace("]}",","+change().replace("bash","libc6").replace("\"reason\":1","\"reason\":2")+"]}");
        PackageBrokerProtocol.Metadata review=review(two);
        assertEquals(HASH,review.digest);assertEquals(2000,review.validUntil);review.requireIntent(intent());
        assertEquals(2,review.changes.size());assertEquals("libc6",review.changes.get(1).name);
        assertEquals(2,review.changes.get(1).reason);
        try { review.changes.clear();fail(); } catch (UnsupportedOperationException expected) { }
        denied(()->review.requireIntent(new PackageBrokerProtocol.Intent(1,1,"bash","5.2")));
        denied(()->review.requireIntent(new PackageBrokerProtocol.Intent(1,2,"bash","5.1")));
    }
    @Test public void ambiguousOrIncompleteReviewCannotBecomeAnApprovalPrompt() {
        for(String invalid:new String[]{review().replace("\"reason\":1","\"reason\":0"),
                review().replace("\"arm64\"","\"amd64\""),review().replace("\"after\":\"5.2\"","\"after\":\"5.1\""),
                review().replace("\"name\":\"bash\"","\"name\":\"sh\""),review().replace(HASH,""),
                review().replace("2000","0"),review().replace("["+change()+"]","[]"),
                review().replace("]}",","+change()+"]}")})denied(()->review(invalid));
        String[] tooMany=new String[65];Arrays.fill(tooMany,change());
        denied(()->review("{"+common()+",\"changes\":["+String.join(",",tooMany)+"]}"));
    }
    @Test public void strictJsonRejectsDuplicatesUnknownFieldsWrongTypesAndTrailingObjects() {
        for(String invalid:new String[]{review().replace("\"action\":1","\"action\":1,\"action\":2"),
                review().replace("\"reason\":1","\"reason\":1,\"reason\":2"),
                review().replace("\"reason\":1","\"reason\":\"1\""),review().replace("\"reason\":1","\"reason\":1.0"),
                review().replace("2000","2e3"),review().replace("2000","9223372036854775808"),
                review().replace("2000","null"),review().replace("2000","true"),
                review().replace("\"scope\":2","\"scope\":2,\"path\":\"/tmp\""),review()+"{}",
                review().replace("\"changes\":[","\"changes\":[[[[")})denied(()->review(invalid));
    }
    @Test public void invalidUtf8IsNeverSilentlyReplaced() {
        byte[] data=frame(10,0,44,2,review());data[49]=(byte)0xff;
        denied(()->parse(data,10,44));
    }
    @Test public void intermediateStatusCannotClaimPublishedGeneration() {
        assertEquals(6,status(status()).phase);
        String published=status().replace("\"phase\":6","\"phase\":10").replace("\"outcome\":0","\"outcome\":3")
                .replace("\"image\":\"\"","\"image\":\""+HASH+"\"").replace("\"bytes\":0","\"bytes\":4096");
        assertEquals(HASH,status(published).image);
        for(String invalid:new String[]{published.replace("\"phase\":10","\"phase\":9"),
                published.replace("\"error\":0","\"error\":5"),published.replace("\"bytes\":4096","\"bytes\":4097"),
                published.replace("\"scope\":2","\"scope\":1"),published.replace("\"outcome\":3","\"outcome\":0"),
                status().replace("\"phase\":6","\"phase\":12"),status().replace(HASH,"")})denied(()->status(invalid));
    }
}
