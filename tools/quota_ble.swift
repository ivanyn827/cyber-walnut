import Foundation
import CoreBluetooth

// A small native transport only. Account authentication and HMAC keys never enter it.
let serviceID = CBUUID(string: "d593f000-7b1e-4c58-9da0-0a12e4c6d216")
let infoID = CBUUID(string: "d593f001-7b1e-4c58-9da0-0a12e4c6d216")
let dataID = CBUUID(string: "d593f002-7b1e-4c58-9da0-0a12e4c6d216")
let ackID = CBUUID(string: "d593f003-7b1e-4c58-9da0-0a12e4c6d216")
func emit(_ s: String) { print(s); fflush(stdout) }
class Link: NSObject, CBCentralManagerDelegate, CBPeripheralDelegate {
    var central: CBCentralManager!
    var peer: CBPeripheral?
    var writer: CBCharacteristic?, ack: CBCharacteristic?
    var chunks: [Data] = []
    var busy = false
    var deadline = Date.distantFuture
    var expectedDevice = ""
    var knownPeer: UUID?
    var nextRetry = Date.distantPast
    var cancelling = false
    var verified = false
    var rejected = Set<UUID>()
    override init() {
        super.init()
        if CommandLine.arguments.count > 1 { expectedDevice = CommandLine.arguments[1] }
        if CommandLine.arguments.count > 2 { knownPeer = UUID(uuidString: CommandLine.arguments[2]) }
        central = CBCentralManager(delegate: self, queue: .main)
        Timer.scheduledTimer(withTimeInterval: 2, repeats: true) { _ in
            if Date() > self.deadline, let p = self.peer {
                if self.cancelling { emit("TRANSPORT cancel_timeout_restart"); exit(2) }
                emit("TRANSPORT timeout_cancel")
                self.cancelling=true; self.deadline=Date().addingTimeInterval(4)
                self.central.cancelPeripheralConnection(p)
            } else if self.peer == nil && Date() >= self.nextRetry && self.central.state == .poweredOn {
                self.nextRetry=Date().addingTimeInterval(15)
                if let id=self.knownPeer, !self.rejected.contains(id),
                   let p=self.central.retrievePeripherals(withIdentifiers:[id]).first {
                    self.connect(p, reason:"known_peer")
                } else { self.scan() }
            }
        }
    }
    func scan() {
        guard central.state == .poweredOn else { return }
        central.stopScan()
        central.scanForPeripherals(withServices: [serviceID], options: nil)
        emit("TRANSPORT scanning")
    }
    func connect(_ p: CBPeripheral, reason: String) {
        guard peer == nil else { return }
        emit("TRANSPORT connecting_\(reason)")
        peer=p; p.delegate=self; verified=false; cancelling=false
        central.stopScan(); deadline=Date().addingTimeInterval(12)
        central.connect(p, options:nil)
    }
    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        emit("STATE \(central.state.rawValue)")
        if central.state == .poweredOn { nextRetry = .distantPast; scan() }
        else { clear() }
    }
    func centralManager(_ central: CBCentralManager, didDiscover p: CBPeripheral, advertisementData: [String: Any], rssi RSSI: NSNumber) {
        if rejected.contains(p.identifier) { return }
        guard peer == nil else { return }
        connect(p, reason:"advertisement")
    }
    func centralManager(_ central: CBCentralManager, didConnect p: CBPeripheral) { guard p === peer else{return}; p.discoverServices([serviceID]) }
    func clear() { peer=nil; writer=nil; ack=nil; chunks=[]; busy=false; verified=false; cancelling=false; deadline = .distantFuture; nextRetry=Date().addingTimeInterval(3); emit("DISCONNECTED") }
    func centralManager(_ central: CBCentralManager, didFailToConnect p: CBPeripheral, error: Error?) { guard p === peer else{return}; emit("TRANSPORT connect_failed"); clear(); scan() }
    func centralManager(_ central: CBCentralManager, didDisconnectPeripheral p: CBPeripheral, error: Error?) { guard p === peer else{return}; emit("TRANSPORT disconnected"); clear(); scan() }
    func peripheral(_ p: CBPeripheral, didDiscoverServices error: Error?) {
        guard p === peer && !cancelling else{return}
        guard error == nil, let s = p.services?.first(where: { $0.uuid == serviceID }) else { central.cancelPeripheralConnection(p); return }
        p.discoverCharacteristics([infoID,dataID,ackID], for: s)
    }
    func peripheral(_ p: CBPeripheral, didDiscoverCharacteristicsFor s: CBService, error: Error?) {
        guard p === peer && !cancelling else{return}
        writer=s.characteristics?.first(where: {$0.uuid == dataID}); ack=s.characteristics?.first(where: {$0.uuid == ackID})
        if let info=s.characteristics?.first(where: {$0.uuid == infoID}), writer != nil, ack != nil { p.readValue(for: info) }
        else { central.cancelPeripheralConnection(p) }
    }
    func peripheral(_ p: CBPeripheral, didUpdateValueFor c: CBCharacteristic, error: Error?) {
        guard p === peer && !cancelling else{return}
        guard error == nil, let data=c.value, let value=String(data:data,encoding:.utf8) else { central.cancelPeripheralConnection(p); return }
        if c.uuid == infoID {
            let parts=value.split(separator:" ")
            guard parts.count == 3, parts[0] == "QHELLO", String(parts[1]) == expectedDevice else {
                emit("TRANSPORT identity_mismatch")
                rejected.insert(p.identifier); central.cancelPeripheralConnection(p); return
            }
            knownPeer=p.identifier;verified=true
            deadline = .distantFuture; emit("PEER \(p.identifier.uuidString) \(value)")
        }
        else if c.uuid == ackID { busy=false; deadline = .distantFuture; emit("ACK \(value)") }
    }
    func send(_ text: String) {
        guard let p=peer, verified, !cancelling, writer != nil, !busy else { return }
        let data=Data(("\n"+text+"\n").utf8), count=min(180,p.maximumWriteValueLength(for:.withResponse))
        chunks=stride(from:0,to:data.count,by:count).map { data.subdata(in:$0..<min($0+count,data.count)) }
        busy=true; deadline=Date().addingTimeInterval(8); next()
    }
    func next() {
        guard let p=peer, let c=writer else { return }
        if !chunks.isEmpty { p.writeValue(chunks.removeFirst(),for:c,type:.withResponse) }
        else { DispatchQueue.main.asyncAfter(deadline:.now()+0.15) { if p === self.peer, !self.cancelling, let a=self.ack { p.readValue(for:a) } } }
    }
    func peripheral(_ p: CBPeripheral, didWriteValueFor c: CBCharacteristic, error: Error?) {
        guard p === peer && !cancelling else{return}
        if error != nil { central.cancelPeripheralConnection(p) } else { next() }
    }
}
let link=Link()
DispatchQueue.global().async {
    while let line=readLine() { DispatchQueue.main.async { link.send(line) } }
    exit(0)
}
RunLoop.main.run()
