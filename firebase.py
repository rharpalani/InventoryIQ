import firebase_admin
from firebase_admin import credentials, db

cred = credentials.Certificate("/Users/rohanharpalani/Desktop/ECE445/server-d0dcc-firebase-adminsdk-edqqp-55c45cc2fa.json")
firebase_admin.initialize_app(cred, {
    'databaseURL': 'https://server-d0dcc-default-rtdb.firebaseio.com/'
})

def set_inventory():
    try:
        data = {
            "in"  : {
                "Wrench" : True,
                "Screwdriver" : True,
                "10 kOhm Resistor" : True,
                "Oscilloscope" : True,
                "9V Battery" : True,
                "LED" : True
            },
            "out" : {
                
            }
        }

        db.reference("inventory").set(data)
        print("success")
    except Exception as e:
        print("Database push failed")
        print(str(e))

def set_users():
    try:

        data = {
            "authorized"  : {
                "Rohan" : True,
                "Rushil" : True,
            },
            "unauthorized" : {
                "Krish" : True,
            }
        }

        db.reference("users").set(data)
        print("success")
    except Exception as e:
        print("Database push failed")
        print(str(e))


def set_checkouts():
    try:

        data = {
            "Rohan" : {
                "wrench" : True,
            }
        }

        db.reference("checkouts").set(data)

        print("Database push successful")
    except Exception as e:
        print("Database push failed")
        print(str(e))


def check_component(component):
    try:
        if db.reference(f'inventory/out/{component}').get():
            print("component present")
        else:
            print("component not present")
    except Exception as e:
        print("Database push failed")
        print(str(e))

def borrow_component(name, component):
    try:
        data = {
            f'{component}' : True
        }

        # add to personal checkouts
        db.reference(f'checkouts/{name}').update(data)

        # add to out and remove from in
        db.reference(f'inventory/out').update(data)
        db.reference(f'inventory/in').child(component).delete()

    except Exception as e:
        print("Database push failed")
        print(str(e))

def return_component(name, component):
    try:
        data = {
            f'{component}' : True
        }

        # remove from checkouts
        db.reference(f'checkouts/{name}').child(component).delete()

        # return from out and add to in
        db.reference(f'inventory/out').child(component).delete()
        db.reference(f'inventory/in').update(data)
        print("success")
    except Exception as e:
        print("Database push failed")
        print(str(e))

def send_component(component):
    try:
        data = {
            "component" : component
        }

        db.reference("current").set(data)

        print(component + " decoded by camera")
    except Exception as e:
        print("Database push failed")
        print(str(e))

def empty_component():
    try:
        db.reference("current/component").delete()

        print("success")
    except Exception as e:
        print("Database push failed")
        print(str(e))

def get_component():
    try:
        print(db.reference("current/component").get())

        print("success")
    except Exception as e:
        print("Database push failed")
        print(str(e))


# set_inventory()
# set_users()
# borrow_component("Rohan", "LED")
# return_component("Rohan", "LED")
send_component("LED")
# get_component()
# empty_component()
