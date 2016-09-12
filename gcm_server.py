
#!/usr/bin/python
import sys, json, xmpp, random, string
from subprocess import PIPE, Popen
from threading  import Thread
from Queue import Queue, Empty
import datetime

SERVER = 'gcm.googleapis.com'
PORT = 5235
USERNAME = "632877030162"
PASSWORD = "AIzaSyBnar8mPqv6CmZ8gqOQBtUOOyq28nw2cwY"
#REGISTRATION_ID = "d3Z--G_Ilq0:APA91bE5qwn034sII-Sm5IkZdsUWfNfBVQVjbcNKDDGcicjI9G7SCWmeuri7o_D99NlymrbLN-JlbyKcSrUfR2iW_-1jShrIILnX_2e0QBsrgBPHb0__pALCc85oHhX82U7iuUv0kDKi"
REGISTRATION_ID = "fkDwv4iOT74:APA91bFSC8ANDGgnimDl6DpWXV4og8cXl54Oj3xluTPmgs8dOI43iU7sCwEMmOljNqoILsHmD3VsZ3WGd-iuTa0H-ab0oWZOJTbqztysUzURwvpCdM02ww7BdcWqsS4fHhPAPG_W9leB"
ON_POSIX = 'posix' in sys.builtin_module_names

unacked_messages_quota = 100
send_queue = []

def UtcNow():
    now = datetime.datetime.utcnow()
    return (now - datetime.datetime(1970, 1, 1)).total_seconds()

# Return a random alphanumerical id
def random_id():
  rid = ''
  for x in range(8): rid += random.choice(string.ascii_letters + string.digits)
  return rid

def message_callback(session, message):
  server_time = UtcNow()
  gcm = message.getTags('gcm')
  if gcm:
    gcm_json = gcm[0].getData()
    msg = json.loads(gcm_json)
    if("message_type" not in msg):
      app_time = int(msg["data"]["time_test"])
      print("Commnication took : " + str((server_time-app_time)) + " seconds")
      print("confirm")

  '''
  global unacked_messages_quota
  gcm = message.getTags('gcm')
  if gcm:
    gcm_json = gcm[0].getData()
    msg = json.loads(gcm_json)
    if not msg.has_key('message_type'):
      # Acknowledge the incoming message immediately.
      send({'to': msg['from'],
            'message_type': 'ack',
            'message_id': msg['message_id']})
      # Queue a response back to the server.
      if msg.has_key('from'):
        # Send a dummy echo response back to the app that sent the upstream message.
        send_queue.append({'to': msg['from'],
                           'message_id': random_id(),
                           'data': {'pong': 1}})
    elif msg['message_type'] == 'ack' or msg['message_type'] == 'nack':
      unacked_messages_quota += 1
    '''

def send(json_dict):
  template = ("<message><gcm xmlns='google:mobile:data'>{1}</gcm></message>")
  client.send(xmpp.protocol.Message(
      node=template.format(client.Bind.bound[0], json.dumps(json_dict))))

def flush_queued_messages():
  global unacked_messages_quota
  while len(send_queue) and unacked_messages_quota > 0:
    send(send_queue.pop(0))
    unacked_messages_quota -= 1

client = xmpp.Client('gcm.googleapis.com', debug=['socket'])
client.connect(server=(SERVER,PORT), secure=1, use_srv=False)
auth = client.auth(USERNAME, PASSWORD)
if not auth:
  print 'Authentication failed!'
  sys.exit(1)

client.RegisterHandler('message', message_callback)

'''send_queue.append({'to': REGISTRATION_ID,
                   'message_id': 'reg_id',
                   'data': {'message_destination': 'RegId',
                            'message_id': random_id()}})'''
def enqueue_output(out, queue):
      for line in iter(out.readline, b''):
          queue.put(line)
      out.close()

q = Queue()
t = Thread(target=enqueue_output, args=(sys.stdin, q))
t.daemon = True # thread dies with the program
t.start()



while True:
  client.Process(1)
  flush_queued_messages()

  # read line without blocking
  try:  line = q.get_nowait() # or q.get(timeout=.1)
  except Empty:
      continue

  if("send" in line):
    print(UtcNow())
    send_queue.append({'to': REGISTRATION_ID,
                   'message_id': 'reg_id',
                   'data': {'message_destination': 'RegId',
                            'message_id': random_id(),
                            'time_test': str(UtcNow())}})







 


  