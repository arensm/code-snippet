// Universelle Halterung für alles
// Universal bracket for everything

// Dicke von nicht-strukturellen Teilen
// thickness of non-structural parts
THK = 4; 

// Dicke der strukturellen Teile
// thickness of structural parts
thk = 3; 

// Breite (links-rechts) des Hubs
// left-right of the hub
x = 63; 

// Höhe (oben-unten) des Hubs
// tleft-right of the hub
z = 60; 

// Tiefe (vorne-hinten) des Teils
// front-back distance of the part
y = 15; 

// Durchmesser der Schraubenachse
// diameter of the screw shaft
screwD = 4;

// Durchmesser des Schraubenkopfs
// diameter of the screw head
headD = 10;

// Art des Schraubenkopfs
// type of screww head
headType = "BUTTON"; //[BUTTON, TAPER]

// Qualität der Rundungen (Anzahl der Segmente)
// quality of curved surfaces
$fn=30;

for(m=[0:1])
mirror([m,0,0])
difference() {
    union() {
        // Frontplatte
        // front panel
        hull() {
            cube([x/2+thk, THK, z+thk]);
            translate([x/4,0]) cube([x/4+thk, THK, z+thk]);
        }
        
        // Montage-Lasche
        // mounting tab
        translate([x/2,0]) cube([thk+headD, y+THK, thk]); //square screw tabs
        hull() {
            translate([x/2,0]) cube([thk, y+THK, thk]);
            translate([x/2+thk+headD/2, (THK+y)/2]) cylinder(d=headD, h=thk);
        }
        
        // Seitenplatte
        // side panel
        translate([x/2,0]) hull() {
            cube([thk, y+THK, z+thk]);
            cube([thk, y+THK, thk]);
        }
        
        // Fixierlasche oben
        // holddown tab
        hull() {
            translate([x/4, 0, z]) cube([x/4, THK, thk]);
            translate([x/2, 0, z]) cube([thk,y+THK,thk]);
        }
            
    }
    
    // Schraubenlöcher
    // screw holes
    translate([x/2+thk+headD/2, (THK+y)/2]) cylinder(d=screwD, thk*3, center=true);
    if(headType=="TAPER") 
    translate([x/2+thk+headD/2, (THK+y)/2,thk+0.01]) rotate([0,180]) cylinder(d1=headD,d2=0, h=headD/2);
    
    // zentrales Loch
    // center hole
    translate([0,0,z/2]) cube([x-2*THK, 99, z-THK],center=true);
}

echo(str("File name: Universal U Bracket ",x,"x",z));
