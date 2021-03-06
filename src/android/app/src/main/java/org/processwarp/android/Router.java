package org.processwarp.android;

import android.os.Build;
import android.os.Handler;
import android.util.Log;

import junit.framework.Assert;

import org.processwarp.android.constant.Module;
import org.processwarp.android.constant.NID;

import java.security.MessageDigest;
import java.util.concurrent.locks.ReentrantLock;

public class Router implements Runnable {
    /**
     * Delegate for implement by service.
     */
    public interface Delegate {
        void routerChangeConnectStatus(Router caller, boolean isConnect, String myNid);
        void routerCreateVm(Router caller, String pid, long rootTid, long procAddr,
                            String masterNid, String name);
        void routerCreateGui(Router caller, String pid);
        void routerRelayControllerPacket(Router caller, CommandPacket packet);
        void routerRelayGuiPacket(Router caller, CommandPacket packet);
        void routerRelayWorkerPacket(Router caller, CommandPacket packet);
    }

    /** Interval to call Scheduler::execute.(sec) */
    private static final int SCHEDULER_EXECUTE_INTERVAL = 3;
    private Delegate delegate;
    private ServerConnector server;
    /** This node's id. */
    private String myNid = null;
    /** Mutex for execute native methods serial. */
    private ReentrantLock lock = new ReentrantLock();
    /** Handler for run loop. */
    private Handler handler;

    /**
     * Initialize some module.
     * @param delegate Delegate for implement by service.
     * @param server ServerConnector instance for send data.
     */
    public void initialize(Delegate delegate, ServerConnector server) {
        this.delegate = delegate;
        this.server = server;

        lock.lock();
        try {
            schedulerInitialize(this);
        } finally {
            lock.unlock();
        }

        // Setup timer to call schedulerExecute.
        handler = new Handler();
        handler.postDelayed(this, SCHEDULER_EXECUTE_INTERVAL * 1000);
    }

    /**
     * Connect server, make password SHA256 digest and send to server to check account.
     * @param account Target account name.
     * @param password Target password.
     */
    public void connectServer(String account, String password) {
        try {
            String hexString = null;
            // apply digest 10 times by SHA256
            byte[] digest = password.getBytes("UTF-8");
            for (int i = 0; i < 10; i ++) {
                MessageDigest md = MessageDigest.getInstance("SHA-256");
                md.update(digest);
                digest = md.digest();

                // Convert to hex string.
                StringBuilder hex = new StringBuilder();
                for (byte b : digest) {
                    if ((b & 0xFF) <= 0x0F) hex.append("0");
                    hex.append(Integer.toHexString(b & 0xFF));
                }
                hexString = hex.toString();
                digest = hexString.getBytes("UTF-8");
            }

            // Send to server
            server.sendConnectNode(account, "[10sha256]" + hexString);

        } catch (Exception e) {
            // TODO error
            Log.e(this.getClass().getName(), "connectServer", e);
            Assert.fail();
        }
    }

    /**
     * Get this node's node-id.
     * @return This node's node-id.
     */
    public String getMyNid() {
        Assert.assertNotNull(myNid);
        Assert.assertFalse("".equals(myNid));

        return myNid;
    }

    /**
     * Is GUI for process in this node?
     * @param pid Target process-id.
     * @return True if GUI is in this node.
     */
    public boolean isGuiInThisNode(String pid) {
        return myNid.equals(schedulerGetDstNid(pid, Module.GUI));
    }

    /**
     * When connect is success, send bind_node packet with my-nid and node-name.
     * @param result Result code.
     */
    public void recvConnectNode(int result) {
        if (result == 0) {
            server.sendBindNode(myNid, Build.HOST);

        } else {
            // If connection is failed, tell connect status.
            delegate.routerChangeConnectStatus(this, false, NID.NONE);
        }
    }

    /**
     * When bind is success, store assigned nid as my-nid and tell changing connect status to another module.
     * @param result Result code.
     * @param nid Assigned nid for this node.
     */
    public void recvBindNode(int result, String nid) {
        if (result == 0) {
            myNid = nid;
            String name = Build.MODEL + "(" + Build.HOST + ")";
            lock.lock();
            try {
                Assert.assertNotNull(nid);
                schedulerSetNodeInformation(nid, name);
            } finally {
                lock.unlock();
            }

            // Tell changing connect status to another module.
            delegate.routerChangeConnectStatus(this, true, myNid);

        } else {
            // If connection is failed, tell connect status.
            delegate.routerChangeConnectStatus(this, false, NID.NONE);
        }
    }

    /**
     * When passed a command from some module in this node,
     * set destination node-id and pass content to capable module in this node or
     * another node through the server.
     * @param packet Command packet.
     * @param isFromServer Set true if packet was passed by server.
     */
    public void relayCommand(CommandPacket packet, boolean isFromServer) {
        handler.post(new Runnable() {
            private Router caller;
            private CommandPacket packet;
            private boolean isFromServer;

            public Runnable initialize(Router caller, CommandPacket packet, boolean isFromServer) {
                this.caller = caller;
                this.packet = packet;
                this.isFromServer = isFromServer;

                return this;
            }

            @Override
            public void run() {
                // Update to real node-id if packet is from another modules in this node.
                if (!isFromServer) {
                    if (NID.THIS.equals(packet.dstNid)) {
                        packet.dstNid = myNid;

                    } else if (NID.NONE.equals(packet.dstNid)) {
                        lock.lock();
                        try {
                            Assert.assertNotNull(packet.pid);
                            packet.dstNid = schedulerGetDstNid(packet.pid, packet.module);
                        } finally {
                            lock.unlock();
                        }
                    }
                    packet.srcNid = myNid;
                }

                // Relay command packet to capable module, if destination node is this node.
                if (packet.dstNid.equals(myNid) || packet.dstNid.equals(NID.BROADCAST)) {
                    switch (packet.module) {
                        case Module.MEMORY:
                        case Module.VM:
                            delegate.routerRelayWorkerPacket(caller, packet);
                            break;

                        case Module.SCHEDULER:
                            lock.lock();
                            try {
                                Assert.assertNotNull(packet.pid);
                                Assert.assertNotNull(packet.dstNid);
                                Assert.assertNotNull(packet.srcNid);
                                Assert.assertNotNull(packet.content);
                                schedulerRecvCommand(
                                        packet.pid, packet.dstNid, packet.srcNid,
                                        packet.module, packet.content);
                            } finally {
                                lock.unlock();
                            }
                            break;

                        case Module.CONTROLLER:
                            delegate.routerRelayControllerPacket(caller, packet);
                            break;

                        case Module.GUI:
                            delegate.routerRelayGuiPacket(caller, packet);
                            break;

                        default:
                            // TODO
                            Assert.fail();
                            return;
                    }
                }

                // Relay command packet by through server,
                // if destination node isn't this node and not from server.
                if (!packet.dstNid.equals(myNid) && !isFromServer) {
                    server.sendRelayCommand(packet);
                }
            }
        }.initialize(this, packet, isFromServer));
    }

    /**
     * When handler's a timer event is happen, call schedulerExecute and reset the timer.
     */
    @Override
    public void run() {
        schedulerExecute();
        handler.postDelayed(this, SCHEDULER_EXECUTE_INTERVAL * 1000);
    }

    /**
     * When scheduler require to create vm, do it by the android service.
     * @param pid Process-id for new vm.
     * @param rootTid Root thread-id for new vm.
     * @param procAddr Process information address for new vm.
     * @param masterNid Master node-id for new vm.
     * @param name Process name for new vm.
     */
    public void routerCreateVm(String pid, long rootTid, long procAddr,
                                  String masterNid, String name) {
        delegate.routerCreateVm(this, pid, rootTid, procAddr, masterNid, name);
    }

    /**
     * When scheduler require to create gui, do it by the android service.
     * @param pid Process-id to bundle to gui.
     */
    public void routerCreateGui(String pid) {
        delegate.routerCreateGui(this, pid);
    }

    /**
     * When scheduler require send a command, relay command by command type.
     * @param pid Process-id bundled to packet.
     * @param dstNid Destination node-id.
     * @param srcNid Source node-id, should be my node-id, THIS, or NONE.
     * @param module Target module.
     * @param content Packet content string of JSON.
     */
    private void routerSendCommand(String pid, String dstNid, String srcNid,
                                      int module, String content) {
        CommandPacket packet = new CommandPacket();
        packet.pid = pid;
        packet.dstNid = dstNid;
        packet.srcNid = srcNid;
        packet.module = module;
        packet.content = content;
        relayCommand(packet, false);
    }

    private native void schedulerInitialize(Router router);
    private native String schedulerGetDstNid(String pid, int module);
    private native void schedulerRecvCommand(String pid, String dstNid, String srcNid,
                                             int module, String content);
    private native void schedulerSetNodeInformation(String nid, String name);
    private native void schedulerExecute();
}
