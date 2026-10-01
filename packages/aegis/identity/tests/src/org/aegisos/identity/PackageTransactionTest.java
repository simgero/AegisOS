package org.aegisos.identity;

import static org.junit.Assert.*;
import android.os.Bundle;
import android.system.OsConstants;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;
import org.junit.Test;

public final class PackageTransactionTest {
    private static final String HASH = "a".repeat(64);
    private static final long DEADLINE = 9999999;
    private static Map<String,Object> common() {
        Map<String,Object> m = new HashMap<>();
        m.put("action",1L);m.put("scope",1L);m.put("package","bash");m.put("version","");
        m.put("digest",HASH);m.put("validUntil",2000L);return m;
    }
    private static PackageBrokerProtocol.Metadata review() {
        Map<String,Object> m=common();
        List<Object> changes=new ArrayList<>();
        for(String name:new String[]{"bash","libc6"}) changes.add(Map.of("name",name,"architecture","arm64",
                "before","5.1","after","5.2","reason",name.equals("bash")?1L:2L));
        m.put("changes",changes);return new PackageBrokerProtocol.Metadata(m,true);
    }
    private static PackageBrokerProtocol.Metadata status(int phase,int outcome) {
        Map<String,Object> m=common();
        if(phase<PackageBrokerProtocol.REVIEWED){m.put("digest","");m.put("validUntil",0L);}
        m.put("phase",(long)phase);m.put("outcome",(long)outcome);m.put("waitStatus",0L);m.put("error",0L);
        boolean published=outcome==PackageBrokerProtocol.PUBLISHED;
        m.put("generation",Map.of("image",published?HASH:"","sharedBase",published?HASH:"","bytes",published?4096L:0L));
        return new PackageBrokerProtocol.Metadata(m,false);
    }
    private static void denied(Runnable action) {
        try { action.run();fail("Expected rejection"); } catch(IllegalStateException|SecurityException expected) { }
    }
    private static final class Fixture {
        final ArrayDeque<Integer> ops=new ArrayDeque<>();
        final ArrayDeque<PackageBrokerProtocol.Reply> replies=new ArrayDeque<>();
        final List<Integer> calls=new ArrayList<>();
        final long[] now={1000};
        final PackageTransaction transaction=new PackageTransaction(new AospIdentityBackend.UserKey(10,17),
                new PackageBrokerProtocol.Intent(1,1,"bash",""),(op,job,intent,digest,deadline)-> {
                    calls.add(op);assertEquals((int)ops.removeFirst(),op);assertEquals(DEADLINE,deadline);
                    assertEquals(op==8?0:41,job);
                    if(op==8)assertEquals("bash",intent.name);else assertNull(intent);
                    assertEquals(op==13?HASH:null,digest);
                    return replies.removeFirst();
                },()->now[0]);
        void reply(int op,int error,PackageBrokerProtocol.Metadata data) {
            ops.add(op);replies.add(new PackageBrokerProtocol.Reply(error,41,data));
        }
        void ready() { ready(review()); }
        void ready(PackageBrokerProtocol.Metadata plan) {
            reply(8,0,null);transaction.begin(DEADLINE);
            reply(12,0,status(1,0));reply(9,0,null);transaction.poll(DEADLINE);
            reply(12,0,status(3,0));reply(10,0,plan);transaction.poll(DEADLINE);
            reply(12,0,status(4,0));reply(11,0,null);transaction.poll(DEADLINE);
            reply(12,0,status(6,0));assertEquals("approval_required",transaction.poll(DEADLINE).getString("state"));
        }
    }
    @Test public void completeReviewPrecedesOneHandoffAndOnlyPublishedIsSuccess() {
        Fixture f=new Fixture();f.ready();
        Bundle view=f.transaction.view(false);
        ArrayList<Bundle> changes=view.getParcelableArrayList("changes",Bundle.class);
        assertEquals(2,changes.size());assertEquals("libc6",changes.get(1).getString("name"));
        assertEquals("automatic",changes.get(1).getString("reason"));assertFalse(f.calls.contains(13));
        PackageApproval.Prepared plan=f.transaction.approval();assertEquals(10,plan.privateOwner().id);
        f.reply(13,0,null);f.transaction.start(plan,DEADLINE);
        assertEquals("running",f.transaction.view(false).getString("state"));
        denied(()->f.transaction.start(plan,DEADLINE));
        f.reply(12,0,status(9,0));assertEquals("unconfirmed",f.transaction.poll(DEADLINE).getString("outcome"));
        f.reply(12,0,status(10,3));assertEquals("published",f.transaction.poll(DEADLINE).getString("outcome"));
        assertTrue(f.transaction.retired());assertTrue(f.ops.isEmpty());
    }
    @Test public void mixedReviewReachesApprovalViewWithoutDroppingRemovalOrChangingOwnership() {
        Map<String,Object> metadata=common();
        metadata.put("changes",List.of(
                Map.of("name","app","architecture","arm64","before","1.0","after","","reason",2L),
                Map.of("name","bash","architecture","arm64","before","5.1","after","5.2","reason",1L)));
        Fixture f=new Fixture();f.ready(new PackageBrokerProtocol.Metadata(metadata,true));
        ArrayList<Bundle> changes=f.transaction.view(false).getParcelableArrayList("changes",Bundle.class);
        assertEquals(2,changes.size());assertEquals("app",changes.get(0).getString("name"));
        assertEquals("1.0",changes.get(0).getString("before"));assertEquals("",changes.get(0).getString("after"));
        assertEquals("automatic",changes.get(0).getString("reason"));
        assertEquals("bash",changes.get(1).getString("name"));assertEquals("5.2",changes.get(1).getString("after"));
        assertFalse(f.calls.contains(13));
        PackageApproval.Prepared prepared=f.transaction.approval();
        assertEquals(PackageApproval.Action.INSTALL,prepared.action);
        assertEquals(10,prepared.privateOwner().id);assertEquals(17,prepared.privateOwner().serial);
        assertEquals(HASH,prepared.planSha256);
        // UI copies cannot alter the retained complete plan.
        changes.get(0).putString("after","9.9");changes.clear();
        ArrayList<Bundle> retained=f.transaction.view(false).getParcelableArrayList("changes",Bundle.class);
        assertEquals(2,retained.size());assertEquals("",retained.get(0).getString("after"));
        f.reply(13,0,null);f.transaction.start(prepared,DEADLINE);
        assertEquals("running",f.transaction.view(false).getString("state"));assertTrue(f.ops.isEmpty());
    }
    @Test public void failedBeginKeepsAllocatedJobForExactCleanup() {
        Fixture f=new Fixture();f.reply(8,OsConstants.EIO,null);denied(()->f.transaction.begin(DEADLINE));
        assertTrue(f.transaction.sealed());assertFalse(f.transaction.retired());
        f.reply(14,0,null);f.reply(12,OsConstants.ENOENT,null);f.transaction.cleanup(DEADLINE);
        assertTrue(f.transaction.retired());assertEquals("unconfirmed",f.transaction.view(true).getString("outcome"));
        assertEquals(List.of(8,14,12),f.calls);
    }
    @Test public void lostBeginReplyRemainsOwnedUntilWholeStopWithoutInventingJob() {
        int[] calls={0};
        PackageTransaction t=new PackageTransaction(new AospIdentityBackend.UserKey(10,17),
                new PackageBrokerProtocol.Intent(1,1,"bash",""),(op,job,intent,digest,deadline)->{
                    calls[0]++;throw new IllegalStateException("Lost reply");
                },()->1000);
        denied(()->t.begin(DEADLINE));t.cleanup(DEADLINE);
        assertEquals(1,calls[0]);assertFalse(t.retired());
        denied(()->t.begin(DEADLINE));t.stopped();assertTrue(t.retired());
    }
    @Test public void cancellationFailureCannotAdvancePublicationThroughStatus() {
        Fixture f=new Fixture();f.ready();f.transaction.seal();
        f.reply(14,OsConstants.EBUSY,null);f.transaction.cleanup(DEADLINE);
        assertFalse(f.transaction.retired());assertEquals(Integer.valueOf(14),f.calls.get(f.calls.size()-1));
        f.reply(14,0,null);f.reply(12,0,status(10,1));f.transaction.cleanup(DEADLINE);
        assertTrue(f.transaction.retired());assertEquals("failed",f.transaction.view(true).getString("outcome"));
    }
    @Test public void completedPublicationWinningCancellationIsReportedAsPublished() {
        Fixture f=new Fixture();f.ready();f.reply(13,0,null);f.transaction.start(f.transaction.approval(),DEADLINE);
        f.transaction.seal();f.reply(14,OsConstants.EALREADY,null);f.reply(12,0,status(10,3));
        f.transaction.cleanup(DEADLINE);
        assertTrue(f.transaction.retired());assertEquals("published",f.transaction.view(true).getString("outcome"));
    }
    @Test public void expiredPlanCannotRequestCredentialsOrStart() {
        Fixture f=new Fixture();f.ready();PackageApproval.Prepared p=f.transaction.approval();f.now[0]=2000;
        denied(()->f.transaction.start(p,DEADLINE));assertTrue(f.transaction.sealed());assertFalse(f.calls.contains(13));
    }
    @Test public void otherPreparedObjectCannotRedirectTheRetainedPlan() {
        Fixture f=new Fixture();f.ready();
        PackageApproval.Prepared other=new PackageApproval.Prepared(new AospIdentityBackend.UserKey(11,18),
                PackageApproval.Scope.ALL,PackageApproval.Action.INSTALL,41,HASH);
        denied(()->f.transaction.start(other,DEADLINE));assertFalse(f.calls.contains(13));
    }
    @Test public void changedIntentNeverContinuesOrShowsAnApprovalPlan() {
        Fixture f=new Fixture();f.reply(8,0,null);f.transaction.begin(DEADLINE);
        Map<String,Object> m=common();m.put("package","coreutils");m.put("phase",1L);m.put("outcome",0L);
        m.put("waitStatus",0L);m.put("error",0L);m.put("generation",Map.of("image","","sharedBase","","bytes",0L));
        f.reply(12,0,new PackageBrokerProtocol.Metadata(m,false));
        try {f.transaction.poll(DEADLINE);fail();}catch(IllegalArgumentException expected){}
        assertTrue(f.transaction.sealed());assertEquals(List.of(8,12),f.calls);
    }
    @Test public void wholeStopRetiresWithoutWaitingOnNativeJobMonitor() throws Exception {
        CountDownLatch entered=new CountDownLatch(1),release=new CountDownLatch(1);
        AtomicReference<Throwable> failed=new AtomicReference<>();
        PackageTransaction t=new PackageTransaction(new AospIdentityBackend.UserKey(10,17),
                new PackageBrokerProtocol.Intent(1,1,"bash",""),(op,job,intent,digest,deadline)->{
                    entered.countDown();
                    try {assertTrue(release.await(5,TimeUnit.SECONDS));}catch(InterruptedException e){throw new AssertionError(e);}
                    return new PackageBrokerProtocol.Reply(0,41,null);
                },()->1000);
        Thread worker=new Thread(()->{try{t.begin(DEADLINE);}catch(Throwable failure){failed.set(failure);}});
        worker.start();
        try {assertTrue(entered.await(5,TimeUnit.SECONDS));t.stopped();assertTrue(t.retired());assertTrue(t.sealed());}
        finally {release.countDown();worker.join(5000);}
        assertFalse(worker.isAlive());assertNull(failed.get());
    }
    @Test public void unknownCancelledJobIsClosedButNeverClaimsRollbackOrSuccess() {
        Fixture f=new Fixture();f.ready();f.transaction.seal();f.reply(14,OsConstants.ENOENT,null);
        f.transaction.cleanup(DEADLINE);Bundle b=f.transaction.view(true);
        assertEquals("closed",b.getString("state"));assertEquals("unconfirmed",b.getString("outcome"));
        assertFalse(b.containsKey("changes"));assertTrue(f.transaction.retired());
    }
    @Test public void failedStartReplySealsAndCannotBeReplayed() {
        Fixture f=new Fixture();f.ready();PackageApproval.Prepared p=f.transaction.approval();
        f.reply(13,OsConstants.EIO,null);denied(()->f.transaction.start(p,DEADLINE));
        denied(()->f.transaction.start(p,DEADLINE));assertFalse(f.transaction.retired());
        assertEquals(1,f.calls.stream().filter(op->op==13).count());
    }
    @Test public void sealedSessionNeverStartsPlanningOrShowsPrivateReview() {
        Fixture f=new Fixture();f.reply(8,0,null);f.transaction.begin(DEADLINE);f.transaction.seal();
        denied(()->f.transaction.poll(DEADLINE));assertEquals(List.of(8),f.calls);
        assertFalse(f.transaction.view(true).containsKey("changes"));
    }
}
