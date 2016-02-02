/**
 *  HC-SR04 Based Water Level Sensor
 *
 *  Copyright 2016 Sergey Stoma
 *
 *  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except
 *  in compliance with the License. You may obtain a copy of the License at:
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software distributed under the License is distributed
 *  on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License
 *  for the specific language governing permissions and limitations under the License.
 *
 */
metadata {
	definition (name: "Water Level", namespace: "sergeystoma", author: "Sergey Stoma") {
		capability "Water Sensor"
        capability "Sensor"
        capability "Configuration"
 
		attribute "level", "number"
	}

	simulator {
	}

	tiles(scale:2) {
	    multiAttributeTile(name:"water", type:"generic", width: 6, height: 4) {
			tileAttribute ("device.water", key: "PRIMARY_CONTROL") {
            	attributeState("dry", label:'dry', icon:"st.alarm.water.dry", backgroundColor:"#ffffff")
	            attributeState("wet", label:'wet', icon:"st.alarm.water.wet", backgroundColor:"#53a7c0")
            }
        }
        valueTile("level", "device.level", decoration: "flat", width: 2, height: 2) {
            state("level", label:'${currentValue}cm depth')
        }

		main("water")
        details(["water", "level"])
	}
    
    preferences {
		section {
			input(title: "Set up", description: "Configure how far is the sensor away from the floor, aka zero depth.", displayDuringSetup: true, type: "paragraph", element: "paragraph")
			input("floorLevel", "number", title: "Floor Level", description: "Floor distance in centimeters", range: "0..*", displayDuringSetup: true)
		}
		section {
			input("safeDepth", "number", title: "What is maximum safe water depth?", description: "Safe water depth in centimeters", range: "0..*", displayDuringSetup: true)
		}
	}
}

// Parse events into attributes.
def parse(String description) {
	def events = []
    
	def value = zigbee.parse(description)?.text
    
    if (value != "ping") {
    	def status = value.split(':')
        if (status.length > 1 && status[0] == "level") {
        	def waterLevel = status[1] as int;
            
            if (floorLevel) {
				def floorLevelValue = floorLevel as int
                def depth = floorLevelValue - waterLevel;
                
                if (safeDepth) {
                	def safeDepthValue = safeDepth as int;
                    def waterStatus = depth <= safeDepthValue ? "dry" : "wet";
                    
                    events.push(createEvent(name: "water", value: waterStatus, descriptionText: "$device.displayName is $waterStatus"))
                } else {
                	events.push(createEvent(name: "water", value: "dry", descriptionText: "$device.displayName safe depth not set"))
                }
		
	            events.push(createEvent(name: "level", value: depth, descriptionText: "$device.displayName depth is $depth cm"))                
            } else {
            	events.push(createEvent(name: "level", value: waterLevel, descriptionText: "$device.displayName level is $waterLevel cm"))
            }
        }
    }
    
    if (events.size() > 0) {
    	return events
    }
}

def updateConfiguration() {
    def floor = 0;
    if (floorLevel) {
    	floor = floorLevel as int;
    }
    def depth = 0;
    if (safeDepth) {
    	depth = safeDepth as int;
    }
    
 	def config = "cnfg $floor $depth";
    def msg = zigbee.smartShield(text: config).format();
        
    return msg
}

def updated() {
  	response(updateConfiguration())
}

def configure() {
	response(updateConfiguration())
}