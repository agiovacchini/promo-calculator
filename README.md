
Esempio di url

http://127.0.0.1:9081/kStore/SelfScan/initSession?devId=123456&devReqId=1


==================
Chiamate base:
==================
InitializeSession
/initSession?devId=#{@devId}&devReqId=@reqId

AddCustomer
/addCustomer?devId=#{@devId}&devReqId=#{@reqId}&devSessId=#{@sessionId}&customerId=#{pCardCode}

AddItem
/addItem?devId=#{@devId}&devReqId=#{@reqId}&devSessId=#{@sessionId}&barcode=#{@itemCode}&bcdType=#{@itemType}

VoidItem
/voidItem?devId=#{@devId}&devReqId=#{@reqId}&devSessId=#{@sessionId}&barcode=#{@itemCode}&bcdType=#{@itemType}

GetTotals
/getTotals?devId=#{@devId}&devReqId=#{@reqId}&devSessId=#{@sessionId}

SuspendTransaction
/endSession?devId=#{@devId}&devReqId=#{@reqId}&devSessId=#{@sessionId}&payStationID=#{@posNumber}




==================
Chiamate aggiunte da me:
==================

Informazioni sul negozio
/getStoreInfo

Elenco carrelli
/getCartsList

Intero carrello con intcodes e descrizioni
http://ip_addr/kStore/SelfScan/getAllCart?devId=1001&devReqId=64&devSessId=128


Intero carrello con barcodes al posto delle descrizioni
http://ip_addr/kStore/SelfScan/getAllCartWithBarcodes?devId=1001&devReqId=64&devSessId=128

Informazioni sul prodotto
/getItemInfo
