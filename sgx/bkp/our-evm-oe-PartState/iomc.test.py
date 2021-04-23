import sys
import pexpect
import re
from time import sleep

# Client's tests


def clientTest():
    print('clientTest', end='', flush=True)
    p = pexpect.spawn('make clientrun')
    p.expect('Client successfully initialized')
    p.sendline('exit')
    print(' - OK')


def clientUnknownCommandTest():
    print('clientUnknownCommandTest', end='', flush=True)
    p = pexpect.spawn('make clientrun')
    p.sendline('something on input')
    p.expect('Unknown command')
    p.sendline('exit')
    print(' - OK')

# Server's tests


def serverTest():
    print('serverTest', end='', flush=True)
    p = pexpect.spawn('make run')
    p.expect('Creating account of Operator...')
    p.sendline('gs')
    p.expect('Global state of host contains accounts:')
    p.close()
    print(' - OK')


def iomcFundTest():
    print('iomcFundTest', end='', flush=True)
    p = pexpect.spawn('make run')

    # make sure iomc-recv has balance 0
    sleep(0.5)
    p.sendline('gs')
    p.expect('Global state of host contains accounts:')

    # find line with iomc-receive
    while True:
        line = p.readline().decode('utf-8')
        if line.find('iomc-receive') != -1:
            iomc_recv = re.search('\[[0-9]*\]\s(0x[0-9,a-f]*)', line).group(1)
            balance = re.search('bal=(0x[0-9,a-f]*)', line).group(1)
            if balance != "0x0":
                print(' - ERROR (invalid starter balance)')
                p.close()
                return
            break

    p.sendline('iomc fund 2')

    # test if balance is 2
    sleep(0.3)
    p.sendline('gs')
    p.expect('] ' + iomc_recv)
    line = p.readline().decode('utf-8')
    balance = re.search('bal=(0x[0-9,a-f]*)', line).group(1)
    if balance != '0x2':
        print(' - ERROR (invalid balance after fund)')
        p.close()
        return

    # second fund
    p.sendline('iomc fund 3')

    sleep(0.3)
    p.sendline('gs')
    p.expect('] ' + iomc_recv)
    line = p.readline().decode('utf-8')
    balance = re.search('bal=(0x[0-9,a-f]*)', line).group(1)
    if balance != '0x5':
        print(' - ERROR (invalid balance after second fund)')
        p.close()
        return

    p.close()
    print(' - OK')

# Client - Server communication


def getIomcAddressesTest():
    print('getIomcAddressesTest', end='', flush=True)
    server = pexpect.spawn('make run')
    server.expect('Creating account of Operator...')
    server.sendline('gs')
    server.expect('Global state of host contains accounts:')

    operator = None
    iomc_send = None
    iomc_recv = None

    # Check all addresses
    while not operator or not iomc_send or not iomc_recv:
        line = server.readline().decode('utf-8')

        if line.find('*[') != -1:
            operator = re.search('\[[0-9]*\]\s(0x[0-9,a-f]*)', line).group(1)

        elif line.find('iomc-send') != -1:
            iomc_send = re.search('\[[0-9]*\]\s(0x[0-9,a-f]*)', line).group(1)

        elif line.find('iomc-receive') != -1:
            iomc_recv = re.search('\[[0-9]*\]\s(0x[0-9,a-f]*)', line).group(1)

    client = pexpect.spawn('make clientrun')
    client.sendline('iomc addr')

    client.expect('sendAddr = ' + iomc_send)
    client.expect('recvAddr = ' + iomc_recv)

    client.sendline('exit')
    server.close()

    print(' - OK')


def clientRegistrationTest():
    print('clientRegistrationTest', end='', flush=True)

    server = pexpect.spawn('make run')
    client = pexpect.spawn('make clientrun')

    client.expect('Address = ')
    clientAddr = client.readline().decode('utf-8')[:42]
    # wait for server startup
    sleep(0.5)
    client.sendline('reg')
    client.expect('Message successfuly sended')

    # wait for tx execution on server
    sleep(0.3)

    server.sendline('gs')
    server.expect('Global state of host contains accounts:')
    server.expect(clientAddr)

    client.sendline('exit')
    server.close()

    print(' - OK')

def multipleClientServersTest():
    print('multipleClientServersTest', end='', flush=True)

    server1 = pexpect.spawn('make run')
    server2 = pexpect.spawn('make run2')
    client1 = pexpect.spawn('make clientrun')
    client2 = pexpect.spawn('make clientrun2')

    client1.expect('Address = ')
    client1Addr = client1.readline().decode('utf-8')[:42]

    client2.expect('Address = ')
    client2Addr = client2.readline().decode('utf-8')[:42]

    # wait for servers startup
    sleep(0.5)

    client1.sendline('reg')
    client1.expect('Message successfuly sended')

    client2.sendline('reg')
    client2.expect('Message successfuly sended')

    # wait for tx execution on server
    sleep(0.3)

    server1.sendline('gs')
    server1.expect('Global state of host contains accounts:')
    server1.expect(client1Addr)

    server2.sendline('gs')
    server2.expect('Global state of host contains accounts:')
    server2.expect(client2Addr)

    client1.sendline('exit')
    client2.sendline('exit')
    server1.close()
    server2.close()

    print(' - OK')

# IOMC protocol demonstration


def iomcProtocolTest():
    print('iomcProtocolTest', end='', flush=True)

    # Start programs
    server1 = pexpect.spawn('make run')
    server2 = pexpect.spawn('make run2')
    client1 = pexpect.spawn('make clientrun')
    client2 = pexpect.spawn('make clientrun2')

    # Get client's adresses
    client1.expect('Address = ')
    client1Addr = client1.readline().decode('utf-8')[:42]
    client2.expect('Address = ')
    client2Addr = client2.readline().decode('utf-8')[:42]

    # print('Clients addresses', flush=True)
    # print(client1Addr, flush=True)
    # print(client2Addr, flush=True)

    sleep(0.5)

    # Get iomc addresses
    client1.sendline('iomc addr')
    client1.expect('Message successfuly sended')
    client2.sendline('iomc addr')
    client2.expect('Message successfuly sended')

    client1.expect('sendAddr = ')
    sendAddrServer1 = client1.readline().decode('utf-8')[:42]
    client1.expect('recvAddr = ')
    recvAddrServer1 = client1.readline().decode('utf-8')[:42]

    # print('IOMC addresses server 1', flush=True)
    # print(sendAddrServer1, flush=True)
    # print(recvAddrServer1, flush=True)

    client2.expect('sendAddr = ')
    sendAddrServer2 = client2.readline().decode('utf-8')[:42]
    client2.expect('recvAddr = ')
    recvAddrServer2 = client2.readline().decode('utf-8')[:42]

    # print('IOMC addresses server 2', flush=True)
    # print(sendAddrServer2, flush=True)
    # print(recvAddrServer2, flush=True)

    # Registration of clients
    client1.sendline('reg')
    client1.expect('Message successfuly sended')
    client2.sendline('reg')
    client2.expect('Message successfuly sended')

    # wait for tx execution on server
    sleep(0.2)

    server1.sendline('gs')
    server1.expect('Global state of host contains accounts:')
    server1.expect(client1Addr)
    line = (server1.readline().decode('utf-8'))
    client1Balance = re.search('bal=(0x[0-9,a-f]*)', line).group(1)

    server2.sendline('gs')
    server2.expect('Global state of host contains accounts:')
    server2.expect(client2Addr)
    line = (server2.readline().decode('utf-8'))
    client2Balance = re.search('bal=(0x[0-9,a-f]*)', line).group(1)

    # print('Balance:', flush=True)
    # print(client1Balance, flush=True)
    # print(client2Balance, flush=True)


    # ----------------------------------------------------------
    # ------------------------ Transfer ------------------------
    # ----------------------------------------------------------
    # Transfer parameters
    preimage = '10'
    hashlock = '0xc65a7bb8d6351c1cf70c95a316cc6a92839c986682d98bc35f958f4883f9d2a8'
    amount = '3'

    # 1. Sender start protocol
    client1.sendline('iomc send-init ' + amount + ' ' + client2Addr +
                     ' ' + client2Addr + ' ' + hashlock)
    client1.expect('Message successfuly sended')

    server1.expect('TX with val = ' + amount + ' from = ' +
                   client1Addr + ' to = ' + sendAddrServer1)
    server1.expect('"data": "')
    sendTransferIdServer1 = server1.readline().decode('utf-8')[:66]
    # expected topic
    server1.expect(
        '0xf1f3b8718b4a6ffe3ab3a702d34de78015c3ef6a9d73ab52105611f98d79ca43')
    server1.expect('>> State in Host and Enclave match! <<')
    # print(sendTransferIdServer1)

    # Sender should have decreased balance
    server1.sendline('gs')
    server1.expect('Global state of host contains accounts:')
    server1.expect(client1Addr)
    line = (server1.readline().decode('utf-8'))
    client1BalanceAfter = re.search('bal=(0x[0-9,a-f]*)', line).group(1)
    assert int(client1Balance, base=16) - int(amount) == int(client1BalanceAfter,
                                                             base=16), 'Client has unexpected balance'

    # 2. Receiver call inicialization on their blockchain
    client2.sendline('iomc recv-init ' + client1Addr + ' ' +
                     client1Addr + ' ' + hashlock + ' ' + amount)
    client2.expect('Message successfuly sended')

    server2.expect('TX with val = 0 from = ' +
                   client2Addr + ' to = ' + recvAddrServer2)
    server2.expect('"data": "')
    recvTransferIdServer2 = server2.readline().decode('utf-8')[:66]
    # expected topic
    server2.expect(
        '0x9a7b105e92924f0e1c62614e0921e97f178e8717ceb8118dac1c1ac36e697b5d')
    server2.expect('>> State in Host and Enclave match! <<')
    # print(recvTransferIdServer2)

    # Receiver should have same balance
    server2.sendline('gs')
    server2.expect('Global state of host contains accounts:')
    server2.expect(client2Addr)
    line = (server2.readline().decode('utf-8'))
    client2BalanceAfter = re.search('bal=(0x[0-9,a-f]*)', line).group(1)
    assert int(client2Balance, base=16) == int(
        client2BalanceAfter, base=16), 'Client has unexpected balance'

    # 3. Sender commit transaction
    client1.sendline('iomc send-commit ' +
                     sendTransferIdServer1 + ' ' + preimage)
    client1.expect('Message successfuly sended')
    server1.expect('TX with val = 0 from = ' +
                   client1Addr + ' to = ' + sendAddrServer1)
    server1.expect('"data": "' + sendTransferIdServer1 + '"')
    # expected topic
    server1.expect(
        '0x308637f70356313976c7209b7dc10ea78bbd26991459919b46a50ab9cd0e765f')
    server1.expect('>> State in Host and Enclave match! <<')

    # Coins should move to sink address 0x0
    server1.sendline('gs')
    server1.expect('Global state of host contains accounts:')
    server1.expect('0x0000000000000000000000000000000000000000')
    line = (server1.readline().decode('utf-8'))
    sinkBalance = re.search('bal=(0x[0-9,a-f]*)', line).group(1)
    assert int(sinkBalance, base=16) == int(
        amount), 'Sink address has unexpected balance'

    # 4a) Sender claim transfer (insuficient contract balance)
    client2.sendline('iomc recv-claim ' +
                     recvTransferIdServer2 + ' ' + preimage)
    client2.expect('Message successfuly sended')
    server2.expect('TX with val = 0 from = ' +
                   client2Addr + ' to = ' + recvAddrServer2)
    server2.expect('"data": "' + recvTransferIdServer2 + '"')
    # expected topic - insufficient funds
    server2.expect(
        '0x8af734ce699c38a1e4671809e55b75f8acf550661a7b8d3a5de92719c432c7c3')
    server2.expect('output as 32B hex: 0x0')
    server2.expect('>> State in Host and Enclave match! <<')

    # Receiver should have same balance (because of insufficient funds on contract)
    server2.sendline('gs')
    server2.expect('Global state of host contains accounts:')
    server2.expect(client2Addr)
    line = (server2.readline().decode('utf-8'))
    client2BalanceAfter = re.search('bal=(0x[0-9,a-f]*)', line).group(1)
    assert int(client2Balance, base=16) == int(
        client2BalanceAfter, base=16), 'Client has unexpected balance'

    # 4b) Operator fund recv contract
    server2.sendline('iomc fund ' + amount)
    server2.expect('>> State in Host and Enclave match! <<')
    # Receive contract shopuld have balance
    server2.sendline('gs')
    server2.expect('Global state of host contains accounts:')
    server2.expect(recvAddrServer2)
    line = (server2.readline().decode('utf-8'))
    recvIomcBalanceServer2 = re.search('bal=(0x[0-9,a-f]*)', line).group(1)
    assert int(recvIomcBalanceServer2, base=16) == int(
        amount), 'Recv IOMC contract has unexpected balance'

    # 4c) Sender claim transfer (successfuly)
    client2.sendline('iomc recv-claim ' +
                     recvTransferIdServer2 + ' ' + preimage)
    client2.expect('Message successfuly sended')
    server2.expect('TX with val = 0 from = ' +
                   client2Addr + ' to = ' + recvAddrServer2)
    server2.expect('"data": "' + recvTransferIdServer2 + '"')
    # expected topic - insufficient funds
    server2.expect(
        '0xc1771bf7efa39d0933bc24178af39297790160ef1a15728c8953b8ec31946811')
    server2.expect('output as 32B hex: 0x1')
    server2.expect('>> State in Host and Enclave match! <<')

    # Receiver should receive coins
    server2.sendline('gs')
    server2.expect('Global state of host contains accounts:')
    server2.expect(client2Addr)
    line = (server2.readline().decode('utf-8'))
    client2BalanceAfter = re.search('bal=(0x[0-9,a-f]*)', line).group(1)
    assert int(client2Balance, base=16) + int(amount) == int(
        client2BalanceAfter, base=16), 'Client has unexpected balance'

    client1Balance = client1BalanceAfter
    client2Balance = client2BalanceAfter

    # ----------------------------------------------------------
    # ----------------- Test one more transfer -----------------
    # ----------------------------------------------------------
    # Transfer parameters
    preimage = '10'
    hashlock = '0xc65a7bb8d6351c1cf70c95a316cc6a92839c986682d98bc35f958f4883f9d2a8'
    amountTx2 = '5'

    # 1. Sender start protocol
    client1.sendline('iomc send-init ' + amountTx2 + ' ' + client2Addr +
                     ' ' + client2Addr + ' ' + hashlock)
    client1.expect('Message successfuly sended')

    server1.expect('TX with val = ' + amountTx2 + ' from = ' +
                   client1Addr + ' to = ' + sendAddrServer1)
    server1.expect('"data": "')
    sendTransferIdServer1 = server1.readline().decode('utf-8')[:66]
    # expected topic
    server1.expect(
        '0xf1f3b8718b4a6ffe3ab3a702d34de78015c3ef6a9d73ab52105611f98d79ca43')
    server1.expect('>> State in Host and Enclave match! <<')

    # Sender should have decreased balance
    server1.sendline('gs')
    server1.expect('Global state of host contains accounts:')
    server1.expect(client1Addr)
    line = (server1.readline().decode('utf-8'))
    client1BalanceAfter = re.search('bal=(0x[0-9,a-f]*)', line).group(1)
    assert int(client1Balance, base=16) - int(amountTx2) == int(client1BalanceAfter, base=16), 'Client has unexpected balance'

    # 2. Receiver call inicialization on their blockchain
    client2.sendline('iomc recv-init ' + client1Addr + ' ' +
                     client1Addr + ' ' + hashlock + ' ' + amountTx2)
    client2.expect('Message successfuly sended')

    server2.expect('TX with val = 0 from = ' +
                   client2Addr + ' to = ' + recvAddrServer2)
    server2.expect('"data": "')
    recvTransferIdServer2 = server2.readline().decode('utf-8')[:66]
    # expected topic
    server2.expect(
        '0x9a7b105e92924f0e1c62614e0921e97f178e8717ceb8118dac1c1ac36e697b5d')
    server2.expect('>> State in Host and Enclave match! <<')

    # # TODO not working
    # # 3. Sender commit transaction
    # client1.sendline('iomc send-commit ' +
    #                  sendTransferIdServer1 + ' ' + preimage)
    # client1.expect('Message successfuly sended')
    # server1.expect('TX with val = 0 from = ' +
    #                client1Addr + ' to = ' + sendAddrServer1)
    # server1.expect('"data": "' + sendTransferIdServer1 + '"')
    # # expected topic
    # server1.expect(
    #     '0x308637f70356313976c7209b7dc10ea78bbd26991459919b46a50ab9cd0e765f')
    # server1.expect('>> State in Host and Enclave match! <<')

    # # Coins should move to sink address 0x0
    # server1.sendline('gs')
    # server1.expect('Global state of host contains accounts:')
    # server1.expect('0x0000000000000000000000000000000000000000')
    # line = (server1.readline().decode('utf-8'))
    # sinkBalance = re.search('bal=(0x[0-9,a-f]*)', line).group(1)
    # assert int(sinkBalance, base=16) == int(
    #     amountTx2) + int(amount), 'Sink address has unexpected balance'

    # 4a) Sender claim transfer (insuficient contract balance)
    client2.sendline('iomc recv-claim ' +
                     recvTransferIdServer2 + ' ' + preimage)
    client2.expect('Message successfuly sended')
    server2.expect('TX with val = 0 from = ' +
                   client2Addr + ' to = ' + recvAddrServer2)
    server2.expect('"data": "' + recvTransferIdServer2 + '"')
    # expected topic - insufficient funds
    server2.expect(
        '0x8af734ce699c38a1e4671809e55b75f8acf550661a7b8d3a5de92719c432c7c3')
    server2.expect('output as 32B hex: 0x0')
    server2.expect('>> State in Host and Enclave match! <<')

    # Receiver should have same balance (because of insufficient funds on contract)
    server2.sendline('gs')
    server2.expect('Global state of host contains accounts:')
    server2.expect(client2Addr)
    line = (server2.readline().decode('utf-8'))
    client2BalanceAfter = re.search('bal=(0x[0-9,a-f]*)', line).group(1)
    assert int(client2Balance, base=16) == int(
        client2BalanceAfter, base=16), 'Client has unexpected balance'

    # 4b) Operator fund recv contract
    server2.sendline('iomc fund ' + amountTx2)
    server2.expect('>> State in Host and Enclave match! <<')
    # Receive contract shopuld have balance
    server2.sendline('gs')
    server2.expect('Global state of host contains accounts:')
    server2.expect(recvAddrServer2)
    line = (server2.readline().decode('utf-8'))
    recvIomcBalanceServer2 = re.search('bal=(0x[0-9,a-f]*)', line).group(1)
    assert int(recvIomcBalanceServer2, base=16) == int(
        amountTx2), 'Recv IOMC contract has unexpected balance'

    # 4c) Sender claim transfer (successfuly)
    client2.sendline('iomc recv-claim ' +
                     recvTransferIdServer2 + ' ' + preimage)
    client2.expect('Message successfuly sended')
    server2.expect('TX with val = 0 from = ' +
                   client2Addr + ' to = ' + recvAddrServer2)
    server2.expect('"data": "' + recvTransferIdServer2 + '"')
    # expected topic - insufficient funds
    server2.expect(
        '0xc1771bf7efa39d0933bc24178af39297790160ef1a15728c8953b8ec31946811')
    server2.expect('output as 32B hex: 0x1')
    server2.expect('>> State in Host and Enclave match! <<')

    # Receiver should receive coins
    server2.sendline('gs')
    server2.expect('Global state of host contains accounts:')
    server2.expect(client2Addr)
    line = (server2.readline().decode('utf-8'))
    client2BalanceAfter = re.search('bal=(0x[0-9,a-f]*)', line).group(1)
    assert int(client2Balance, base=16) + int(amountTx2) == int(
        client2BalanceAfter, base=16), 'Client has unexpected balance'

    # EXIT
    client1.sendline('exit')
    client2.sendline('exit')
    server1.close()
    server2.close()

    print(' - OK')


class Tests:
    def testAll(self):
        self.clientTests()
        self.serverTests()
        self.clientServerTests()
        self.iomcTest()

    def clientTests(self):
        clientTest()
        clientUnknownCommandTest()

    def serverTests(self):
        serverTest()
        iomcFundTest()

    def clientServerTests(self):
        getIomcAddressesTest()
        clientRegistrationTest()
        multipleClientServersTest()

    def iomcTest(self):
        iomcProtocolTest()


tests = Tests()
tests.testAll()
