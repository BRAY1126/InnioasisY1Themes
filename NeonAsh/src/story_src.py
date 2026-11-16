"""
NEON ASH -- story source.

Authoring format (compiled by build.py into story.json + neon_ash.html):

  node(id, text, choices=[...], fx="", route=[...], extra=[...])
  ending(id, title, tone, text)
  C(label, to, need="", fx="", hide=False, hideif="", once=False, tag=None)

Conditions ("need", "hideif", route/extra conditions):
  terms joined with & (and) / | (or, lower precedence)
  flag        -> flag is set
  !flag       -> flag is not set
  var>=3      -> numeric compare (>=, <=, >, <, ==, !=)

Effects ("fx"): space separated
  +flag / -flag     set / clear a flag
  var+2 / var-1     add / subtract
  var=0             set

Text: {name} and {anyvar} are substituted at runtime.
"""
import textwrap

NODES = {}
ENDINGS = []


def T(s):
    s = textwrap.dedent(s).strip("\n")
    paras = []
    for p in s.split("\n\n"):
        out = []
        for line in p.splitlines():
            line = line.strip()
            if out and not line.startswith("-- "):
                out[-1] += " " + line
            else:
                out.append(line)
        paras.append("\n".join(out))
    return "\n\n".join(p for p in paras if p)


def C(t, to, need="", fx="", hide=False, hideif="", once=False, tag=None):
    return {"t": t, "to": to, "need": need, "fx": fx, "hide": hide,
            "hideif": hideif, "once": once, "tag": tag}


def node(nid, text="", choices=(), fx="", route=(), extra=()):
    if nid in NODES:
        raise ValueError("duplicate node " + nid)
    NODES[nid] = {"text": T(text), "choices": list(choices), "fx": fx,
                  "route": [list(r) for r in route],
                  "extra": [[c, T(x)] for c, x in extra]}


def ending(nid, title, tone, text, extra=()):
    node(nid, text, extra=extra)
    NODES[nid]["ending"] = {"title": title, "tone": tone}
    ENDINGS.append({"id": nid, "title": title, "tone": tone})


BACK = lambda to="hub", t="Back to the streets.": C(t, to)

# =====================================================================
#  CHARACTER CREATION
# =====================================================================

node("start", """
    PORT HALCYON, 2091.

    A city built on a drowned city. Forty million people, every one of them
    wired into the Lattice through the neural port behind their ear. Rain
    that never quite stops. Light that never quite reaches the ground.

    The megacorp Halvorsen-Mori owns the towers, the water, the police, and
    most of the air. Everyone else owns whatever they can hold onto.

    You've been holding on for a long time, {name}. Where did you come from?
""", [
    C("GUTTER-BORN. Raised in the flooded markets of Rustwater. You know every alley, every debt, every lie.", "o_street"),
    C("SALT RIDER. Grew up on the salt flats beyond the wall, in a convoy clan. Engines, sky, and family.", "o_salt"),
    C("GLASS-BLOOD. A Halvorsen-Mori employee in a tower office. Until last night, when you read something you shouldn't have.", "o_glass"),
])

node("o_street", fx="+street body=2 reflex=3 tech=2 mind=2 nerve=3 pts=6 money=300 humanity=10",
     route=[["", "alloc"]])
node("o_salt", fx="+salt body=3 reflex=2 tech=3 mind=2 nerve=2 pts=6 money=500 humanity=10",
     route=[["", "alloc"]])
node("o_glass", fx="+glass body=2 reflex=2 tech=2 mind=3 nerve=3 pts=6 money=1500 humanity=10 +knows_lullaby",
     route=[["", "alloc"]])

node("alloc", """
    WHO ARE YOU, REALLY?

    Spend your points. Every attribute unlocks different options throughout
    the city. Nothing is wasted; everything closes some doors and opens others.

    -- BODY {body}: strength, toughness, intimidation.
    -- REFLEX {reflex}: speed, guns, driving, getting out alive.
    -- TECH {tech}: hacking, machines, hardware.
    -- MIND {mind}: intelligence, noticing things, reading people.
    -- NERVE {nerve}: cool, charm, lies, keeping your head.

    Points remaining: {pts}  (max 7 per attribute)
""", [
    C("+1 BODY", "alloc", need="pts>=1 & body<7", fx="body+1 pts-1", tag=""),
    C("+1 REFLEX", "alloc", need="pts>=1 & reflex<7", fx="reflex+1 pts-1", tag=""),
    C("+1 TECH", "alloc", need="pts>=1 & tech<7", fx="tech+1 pts-1", tag=""),
    C("+1 MIND", "alloc", need="pts>=1 & mind<7", fx="mind+1 pts-1", tag=""),
    C("+1 NERVE", "alloc", need="pts>=1 & nerve<7", fx="nerve+1 pts-1", tag=""),
    C("Start over.", "alloc_reset"),
    C("Lock it in. Let's go.", "alloc_done", need="pts==0", tag=""),
])
node("alloc_reset", route=[["street", "o_street"], ["salt", "o_salt"], ["", "o_glass"]])
node("alloc_done", route=[["street", "p_street"], ["salt", "p_salt"], ["", "p_glass"]])

# =====================================================================
#  PROLOGUE: GUTTER-BORN
# =====================================================================

node("p_street", """
    RUSTWATER MARKETS. 23:40.

    The noodle stall under the monorail is the only warm place for six blocks.
    You're halfway through a bowl of synth-pork when the rain on the awning
    changes rhythm. Footsteps. Three sets.

    Deacon Ruiz of the Chrome Saints sits down beside you like an old friend.
    His halo -- a ring of polished steel bolted into his skull -- drips
    rainwater onto the counter. Two acolytes stand behind him, very still.

    "Two thousand creds, {name}. Mother Lux was patient. Mother Lux is done
    being patient."

    You have {money}. You do not have two thousand.
""", [
    C("\"I've got a big job lined up. Give me a week and I'll pay double.\"", "p_street_talk", need="nerve>=5"),
    C("Flip the boiling noodle pot into Ruiz's face and run.", "p_street_run", need="reflex>=5"),
    C("Grab the nearest acolyte and break his arm on the counter. Make a point.", "p_street_fight", need="body>=5"),
    C("Push all your money across the counter and beg.", "p_street_beg"),
    C("Go for the pistol in Ruiz's waistband.", "e_wrong_alley"),
])

node("p_street_talk", """
    You don't blink. You tell him about the job -- the one you don't have
    yet -- with enough detail that you almost believe it yourself.

    Ruiz studies you for a long moment, then laughs, a sound like gravel in a
    blender. "Double. In a week. You hear that? The rat's got ambition."

    He taps his halo. "Mother Lux likes ambition. One week, {name}. After
    that, we baptize you in the canal."

    They leave. The noodle vendor exhales for the first time in two minutes.
""", [C("Finish your noodles. Your phone is buzzing.", "street_call")],
     fx="rep_saints+1 +owes_saints")

node("p_street_run", """
    The pot goes up, the broth goes everywhere, and Ruiz goes backwards off his
    stool screaming. You're already moving -- under the counter, through the
    curtain, out the back where the fish crates are stacked.

    Rustwater is a maze and you were raised in it. Ninety seconds later you're
    three levels up a fire escape, listening to the acolytes curse below.

    You've bought time. You've also made an enemy who will remember your face.
""", [C("Catch your breath. Your phone is buzzing.", "street_call")],
     fx="rep_saints-1 heat+1")

node("p_street_fight", """
    The acolyte's arm makes a sound you will hear again in your sleep. He
    folds. The second acolyte reaches for a blade and you put him into the
    noodle vat.

    Ruiz doesn't move. He just smiles, slowly, chrome teeth catching neon.

    "Mother Lux will want to meet you. Not today. But she will." He drops a
    coin on the counter for the noodles and walks out into the rain.

    You're not sure if that was a threat or a job offer. In the Undertow it's
    usually both.
""", [C("Wipe your hands. Your phone is buzzing.", "street_call")],
     fx="rep_saints+1 heat+1 humanity-1")

node("p_street_beg", """
    You put everything on the counter. Every crumpled bill, every chit.
    Your pride goes on there too, though it doesn't count toward the total.

    Ruiz counts it slowly, in front of everyone. Then he pockets it.

    "Interest," he says. "The debt stands. But Mother Lux values humility.
    You've got a week."

    The noodle vendor slides you a fresh bowl, free. Rustwater looks after
    its own, in small ways.
""", [C("Eat. Your phone is buzzing.", "street_call")],
     fx="money=0 +owes_saints humanity+1")

node("street_call", """
    A text. No name, just a silver crescent icon you'd know anywhere.

    VESPER KANE: Heard you need creds. I need hands. Last Light. One hour.
    Come alone and come sober.

    Vesper is the best fixer in Rustwater. Vesper does not call people like
    you. Not unless the job is big -- or the job is bad.
""", [C("Head to the Last Light.", "vesper")])

ending("e_wrong_alley", "WRONG ALLEY", "death", """
    You're fast. Ruiz is chrome.

    His hand closes around your wrist before your fingers touch the grip. He
    sighs, almost sadly, like a teacher with a slow student. The acolytes take
    you out the back.

    They find you three days later in the canal under the old Shenzhen Bridge,
    a little steel halo hammered into your forehead -- the Saints' joke about
    converts.

    The noodle stall plays your favorite song once, the next night, in your
    honor. Then the rain washes the chalk outline off the street, and Rustwater
    forgets you, the way it forgets everyone.
""")

# =====================================================================
#  PROLOGUE: SALT RIDER
# =====================================================================

node("p_salt", """
    THE SALT GATE. 22:15.

    Three years since you left the Riders. Grandmother Asa kissed your
    forehead when you went and said what the old ones always say: "The road
    remembers." You weren't sure, then, if it was a blessing or a warning.

    Now your battered water-hauler idles in a line of trucks at the wall,
    under Lumen Security floodlights. Lumen is Halvorsen-Mori's private police.
    Behind the false panel in your cargo bed: forty doses of anti-rejection
    meds for a chromedoc in Rustwater called Doc Marrow. Black market. Very
    illegal. Very needed.

    A guard in a mirrored visor raps on your window. A scanner drone hums
    toward your cargo.
""", [
    C("Pop the dash and spoof the scanner's handshake with your homemade deck.", "p_salt_tech", need="tech>=5"),
    C("Smile. \"Just a water hauler, officer. Long night for both of us.\"", "p_salt_talk", need="nerve>=5"),
    C("Floor it through the barrier before the drone reaches the bed.", "p_salt_run", need="reflex>=6"),
    C("Fold ¢200 into your permit and hand it over.", "p_salt_bribe", need="money>=200"),
    C("Tell them about the hidden compartment. It's not worth dying for.", "p_salt_surrender"),
    C("Reach under the seat for the shotgun.", "e_gate"),
])

node("p_salt_tech", """
    Your deck is a brick of salvaged circuit boards held together with salt-
    crusted tape, and it is beautiful. You find the drone's handshake protocol,
    feed it a clean manifest, and watch its little light blink green.

    "Water," the guard reads, bored. "Go."

    The wall swallows your truck. Behind you, the salt flats go dark.
""", [C("Drive into the city.", "p_salt_marrow")], fx="+cargo")

node("p_salt_talk", """
    You talk about the drought, the water prices, how the guard's shift must
    be murder. You don't look at the cargo bed once. You laugh at his joke,
    which isn't funny.

    He waves the drone off. "Get out of here, salt."

    You do, slowly, like an innocent person would.
""", [C("Drive into the city.", "p_salt_marrow")], fx="+cargo")

node("p_salt_run", """
    The engine screams. The barrier arm splinters across your windshield. The
    guard dives, the drone spins, and you are through, fishtailing onto the
    elevated highway with sirens howling behind you.

    You lose them in the old freight tunnels -- Riders know every hole in the
    wall. But Lumen has your plates now.
""", [C("Drive into the city.", "p_salt_marrow")], fx="+cargo heat+2")

node("p_salt_bribe", """
    The guard looks at the bills. Looks at you. The money disappears the way
    money always disappears in Port Halcyon -- quietly, into someone else's
    pocket.

    "Welcome to the city," he says, and means none of it.
""", [C("Drive into the city.", "p_salt_marrow")], fx="+cargo money-200")

node("p_salt_surrender", """
    You tell them. They pull the panel, take the meds, take your truck, and
    take two hours of your life in a holding cell. A sergeant gives you a
    lecture about the law, then sells the meds back to a Rustwater dealer
    before his shift ends. You know because you watch him do it.

    They let you go at dawn, on foot, with a fine you'll never pay. At least
    you're inside the wall.
""", [C("Walk into the city.", "salt_call")], fx="money-200 humanity-1")

node("p_salt_marrow", """
    Doc Marrow's clinic is a converted laundromat in Rustwater. Marrow has eight
    arms -- two original, six surgical -- and opens the crate with all of them
    at once.

    "Forty doses. Forty people who don't reject their chrome this month." The
    doc looks up at you with something rare in this city: gratitude. "I owe
    you, Rider. Remember that. I will."
""", [C("Rest. Your phone is buzzing.", "salt_call")], fx="+marrow_friend humanity+1")

node("salt_call", """
    A text from an unknown number. Just a silver crescent.

    VESPER KANE: You're the Rider who just came through the wall. Impressive.
    I have work for someone nobody in the city knows yet. Last Light, Rustwater.
    One hour.

    You've heard the name. Everyone has. Vesper Kane: the fixer who makes
    impossible jobs possible, for a percentage.
""", [C("Head to the Last Light.", "vesper")])

ending("e_gate", "DEAD AT THE GATE", "death", """
    The shotgun is halfway up when the turret on the wall finds you.

    It doesn't make much noise. Lumen Security buys quiet guns.

    They tow your truck to an impound lot and auction the parts. The meds go
    to a dealer. Doc Marrow waits for a delivery that never comes, and forty
    people in Rustwater reject their chrome that month.

    Out on the salt, Grandmother Asa wakes in the night with a cold feeling,
    and lights a candle in the convoy shrine. The road remembers. The city
    doesn't.
""")

# =====================================================================
#  PROLOGUE: GLASS-BLOOD
# =====================================================================

node("p_glass", """
    LUMEN TOWER, FLOOR 61. 08:55.

    You audit compliance for Halvorsen-Mori: you read budgets nobody else
    reads. Last night you read one line too many.

    PROJECT LULLABY. A neural firmware update to be pushed, silently, into
    every port in Port Halcyon. You followed the money for six hours. The
    update doesn't fix anything. It edits. It softens anger, dulls memory,
    nudges forty million people toward calm. Toward compliance.

    At 08:50 your terminal chimed: Elias Thorne, VP of Cognitive Products,
    would like to see you on 90. Immediately.
""", [
    C("Copy the Lullaby files to a shard and tape it inside your shoe before you go.", "p_glass_copy", need="mind>=5"),
    C("Go straight up. Act normal.", "p_glass_thorne"),
])

node("p_glass_copy", """
    Ninety seconds. Your hands are steady; your heart isn't. The shard is the
    size of a fingernail and weighs nothing at all, and you can feel it with
    every step to the elevator.
""", [C("Ride up to 90.", "p_glass_thorne")], fx="+lullaby_files")

node("p_glass_thorne", """
    Thorne's office is mostly sky. He's silver-haired, soft-voiced, and
    pours you tea himself.

    "You're very good at your job," he says. "That's the problem. You saw
    Lullaby." He doesn't ask. "So here's the kindest thing I can offer. Sign
    this, and our clinic downstairs trims the last forty-eight hours from your
    memory. You'll keep your job. You'll get a raise. You'll be happy. I
    promise you'll be happy."

    Two security officers wait outside the glass door. Neither looks at you.
""", [
    C("Sign. Take the trim. Being happy doesn't sound so bad.", "e_clean_slate"),
    C("\"I set up a dead man's switch before I came up. Let me walk.\"", "p_glass_bluff", need="nerve>=5"),
    C("Trigger the fire suppression system from your wrist deck. Walk out in the chaos.", "p_glass_escape", need="tech>=5"),
    C("Put Thorne through his own glass desk.", "p_glass_violent", need="body>=5"),
    C("Bolt for the stairwell.", "p_glass_run"),
])

node("p_glass_bluff", """
    You sip the tea. You tell him exactly where the files will be published if
    you don't check in by noon. Every word is a lie, and you deliver it like a
    quarterly report.

    Thorne's smile doesn't move, but his eyes do. "Severance, then," he says
    softly. "Generous. And we'll be watching you for the rest of your life."

    Security walks you out with a box of your things. You don't breathe until
    you hit the street.
""", [C("Find somewhere to think. Your phone buzzes.", "glass_call")], fx="money+3000 heat+1 rep_corp-1")

node("p_glass_escape", """
    One tap. The ceiling opens and floor 90 vanishes in a white roar of fire-
    suppression foam. Alarms. Shouting. Thorne's tea goes everywhere.

    You walk -- don't run -- to the service stairs, past the guards wiping foam
    from their visors. Twenty-nine floors down, you take the freight exit into
    the rain.
""", [C("Disappear into the crowd. Your phone buzzes.", "glass_call")], fx="heat+1 rep_corp-1")

node("p_glass_violent", """
    The desk is real glass, which surprises you; you'd assumed it was
    polymer, like everything else in this building. Thorne goes through it.
    He's still breathing. Good.

    The guards come in fast. You go out faster, through the side door, down
    the executive elevator. Your badge still works for exactly ninety more
    seconds.
""", [C("Run. Your phone buzzes.", "glass_call")], fx="heat+2 rep_corp-2 humanity-1")

node("p_glass_run", """
    Sixty-one floors of stairs. You make it to the lobby on legs that are
    barely legs anymore, and out the revolving door, and into a cab.

    By the time you reach your apartment, your accounts are frozen, your
    lease is cancelled, and your face is on a Lumen Security watchlist.
    Halvorsen-Mori doesn't fire people. It erases them.
""", [C("Sit in the dark. Your phone buzzes.", "glass_call")], fx="money=0 heat+2 rep_corp-1")

node("glass_call", """
    A text from a number that shouldn't have yours. A silver crescent.

    VESPER KANE: Word travels. Someone inside HM just burned their whole life.
    I like people with nothing left to lose. Last Light, Rustwater. Tonight.

    You've never been to Rustwater. You've only seen it from the 61st floor:
    a smear of neon at the bottom of the rain.
""", [C("Go to Rustwater.", "vesper")])

ending("e_clean_slate", "CLEAN SLATE", "neutral", """
    The clinic is warm and smells like lavender. The procedure takes four
    minutes.

    You wake up at your desk, mildly confused about a missing weekend. Your
    salary went up. Your manager is lovely to you now. Every morning you take
    the elevator to 61, and every evening you go home and feel... fine.

    Seven months later, a firmware update rolls out across the city. You
    feel it like a warm bath. Everyone does. The protests in Rustwater stop.
    The crime rate drops. The news calls it a miracle.

    Sometimes, late at night, you open a budget spreadsheet and feel your
    hands start to shake, and you don't know why. Then the feeling passes,
    and you close the file, and you are happy. Thorne promised you would be.
""")

# =====================================================================
#  ACT ONE: THE JOB
# =====================================================================

node("vesper", """
    THE LAST LIGHT. RUSTWATER.

    The bar is a sunken subway car with a liquor license. Vesper Kane sits in
    the back booth: sharp suit, shaved head, a silver crescent tattooed under
    one eye. Clove smoke. A drink that hasn't been touched.

    Beside Vesper sits a woman with copper braids wired into a runner's
    crown and a grin like she already knows how this ends. "Juno Reyes,"
    Vesper says. "Best runner in Rustwater. Your partner, if you say yes."

    "Lumen Tower," Vesper continues. "R&D, floor 88. A cryo-case marked
    MNEMOSYNE. Bring it to me, and the buyer pays fifteen thousand to each of
    you. Five up front."

    Fifteen thousand. You've never seen fifteen thousand in one place.
""", [
    C("\"What's in the case?\"", "vesper_case", once=True),
    C("\"Tell me about Juno.\"", "vesper_juno", once=True),
    C("\"Fifteen's low for Lumen Tower. Make it twenty-five.\"", "vesper_neg", need="nerve>=6", once=True),
    C("\"I'm in.\"", "vesper_yes"),
    C("\"This smells wrong. I'm out.\"", "e_small_life"),
], extra=[
    ["owes_saints", "You think about Deacon Ruiz and the canal. A week, he said."],
    ["glass", "Lumen Tower. You know its floor plans. Vesper knows you know."],
    ["salt", "City jobs, city money. The Riders would call this foolish. The Riders aren't here."],
])

node("vesper_case", """
    Vesper turns the drink slowly. "The buyer didn't say. The buyer pays enough
    that I didn't ask."

    Juno snorts. "Which means it's either a weapon or a person. HM doesn't
    put anything else in cryo."
""", [C("Back.", "vesper")], extra=[
    ["knows_lullaby", """You go cold. MNEMOSYNE. You saw that word in the Lullaby budget --
        a single line item, eighty billion creds, marked CORE ASSET."""],
])

node("vesper_juno", """
    "I'm right here," Juno says. "Ask me."

    So you do. She grew up in the Undertow, before the Saints took it. Ran with
    a runner crew called the Static for a while. Got out. Has a garage in
    Rustwater and a sister she doesn't talk about.

    "Lia," she says, when you push. "HM ran a 'memory study' in the Undertow
    clinics, two years back. Paid fifty creds a head. Lia went in. Lia came out
    empty. Doesn't know my name now." She smiles without any happiness at all.
    "So, yeah. I'll rob Halvorsen-Mori for free. The fifteen is a bonus."
""", [C("Back.", "vesper")], fx="juno+1")

node("vesper_neg", """
    Vesper's eyes narrow, then crinkle. "Twenty. And I'll remember you
    asked." A thin smile. "Fixers like people who know their price."
""", [C("Back.", "vesper")], fx="+negotiated")

node("vesper_yes", """
    Vesper slides a chit across the table. ¢5,000, already transferred.

    "Tomorrow night. Juno has the plan." The fixer stands, leaves the drink
    untouched, and pauses at the door. "And {name}? If anything about this job
    feels off once you're inside -- it is. Get out."
""", [C("Tomorrow night.", "heist_plan")], fx="money+5000")

ending("e_small_life", "A SMALL LIFE", "neutral", """
    You walk out of the Last Light and into the rain, and you keep walking.

    Two nights later, Lumen Tower goes into lockdown. The newsfeeds show a
    burned-out van and a body bag. You never learn whose.

    You take a job loading cargo at the docks. Then a better one, running a
    forklift. You rent a room with a window that looks at another window. You
    get a cat. You name it after the fixer, as a private joke.

    Seven months later, a firmware update rolls across the city like warm
    fog. Everyone feels calmer. You feel calmer. You stop wondering what was in
    the case.

    It's a small life. It's yours. In Port Halcyon, that counts as winning.
""", extra=[
    ["owes_saints", """The Saints come for their debt one night, in the end. But by then you
        have a little saved, and Deacon Ruiz is in a good mood, and you get to
        keep both your hands. Mostly, small lives are like that."""],
])

node("heist_plan", """
    LUMEN TOWER. 02:10.

    Juno's van is parked under a dead billboard three blocks from the tower,
    rain hammering the roof. She has the floor plans on a cracked holo.

    "Four ways in," she says. "Maintenance roof and the air ducts. The front
    lobby, if you can lie with a straight face. The data trunk -- we both jack
    in and ghost the cameras. Or the freight dock, and we do this ugly."

    She looks at you. "Your call, partner. Or I pick, and you trust me."
""", [
    C("Vents. Juno talks you through from the van.", "h_vent", need="reflex>=5"),
    C("Walk in the front door with a borrowed contractor badge.", "h_lobby", need="nerve>=5 | glass",
      tag="[NERVE 5 / GLASS-BLOOD]"),
    C("Jack in with Juno. Ghost the whole building.", "h_hack", need="tech>=5"),
    C("Freight dock. Go loud.", "h_loud", need="body>=5"),
    C("\"You pick, Juno.\"", "h_juno_pick"),
])

node("h_juno_pick", """
    Juno grins. "Vents. I'll be your eyes." She cracks her knuckles and slides
    on her crown. "But to see that much of the building I have to be a lot
    deeper in their system. If they spot me, they spot me hard."

    It's a good plan. It puts her in more danger than you. She knows. She
    picked it anyway.
""", [C("Up to the roof.", "h_vent_easy")], fx="+juno_exposed juno+1")

node("h_vent_easy", """
    You crawl through the ducts with Juno's voice in your ear: left, left,
    wait, now. She sees every drone before it sees you. Twice she loops a
    camera so you can drop through a grate.

    "Floor 88," she whispers. "Their system's starting to sniff around me.
    Grab it fast."
""", [C("Drop into the lab.", "h_vault")])

node("h_vent", """
    The ducts are hot, tight, and smell of ozone. Juno is a voice in your ear,
    calling turns. Floor 84. 85. 86.

    Then a security drone the size of a fist floats around a corner ahead,
    sweeping red light along the metal. Thirty seconds until it reaches you.
""", [
    C("Time it perfectly and slip past beneath its sweep.", "h_vault", need="reflex>=6"),
    C("Pry open its panel and rewrite its patrol loop.", "h_vault", need="tech>=4", fx="+drone_pet"),
    C("Back up into a junction and wait it out.", "h_vault", fx="+alarm_early"),
    C("Crush it before it can report.", "h_vault", fx="+alarm_early heat+1"),
], extra=[["salt", "You've crawled through worse. Engine blocks in a dust storm are worse."]])

node("h_lobby", """
    The lobby is white marble and soft music. An android receptionist smiles
    at you. A human guard does not.

    "Contractor? At two in the morning?" He holds out his hand for your badge.
""", [
    C("\"Coolant leak on 88. You want to explain a flooded lab to Thorne in the morning, or let me fix it?\"",
      "h_vault", need="nerve>=6"),
    C("Use your old HM jargon. Cite a real work-order number from memory.", "h_vault", need="glass | mind>=5",
      tag="[MIND 5 / GLASS-BLOOD]"),
    C("Hand him Vesper's forged badge and hope.", "e_processed"),
    C("Back out. \"Wrong building, sorry.\" Try the vents.", "h_vent"),
])

node("h_hack", """
    You and Juno plug in side by side in the van. The world goes blue.

    Lumen Tower's network rises around you like a cathedral of light. You
    ghost the cameras floor by floor, rewriting what they see. Easy. Too easy.

    Then, at floor 85, the black wall: a daemon of ICE, coiled around the R&D
    subnet like a sleeping animal made of knives.
""", [
    C("Find the maintenance backdoor its architect forgot to close.", "h_hack_ok", need="tech>=6"),
    C("Study its pattern. Every daemon has a rhythm. Find the gap.", "h_hack_ok", need="mind>=6"),
    C("Let Juno take the ICE hit while you slip through.", "h_hack_ok", fx="+juno_exposed juno-1"),
    C("Brute-force straight through it.", "h_hack_ok", fx="+alarm_early burn+2 humanity-1"),
])

node("h_hack_ok", """
    You're through. The R&D floor opens up like a flower. Doors unlock.
    Cameras loop. Somewhere in the real world, your body walks out of the
    van and into a service entrance nobody is watching.
""", [C("Up to floor 88.", "h_vault")], extra=[
    ["burn>=2", """Your nose is bleeding. Your vision has a white crack running through it.
        Brute-forcing ICE is a good way to leave pieces of yourself behind."""],
])

node("h_loud", """
    Freight dock. Juno kills the lights; you kill the door. Inside, two Lumen
    guards look up from a card game with exactly the expression you'd expect.
""", [
    C("Close the distance and put them both down before they reach a radio.", "h_vault", need="body>=6",
      fx="heat+2 +alarm_early"),
    C("Draw and fire first.", "h_vault", need="reflex>=6", fx="heat+2 +alarm_early humanity-1"),
    C("Kick the door and start spraying.", "e_coffin"),
    C("Abort. Back to the van -- try the vents.", "h_vent"),
])

ending("e_processed", "PROCESSED", "bad", """
    The guard scans the badge. It beeps, politely, the way HM equipment always
    beeps when it ruins your life.

    "Huh," he says, and the lobby doors lock.

    Lumen Security doesn't have courts. It has a process. You're processed in
    a white room under the tower: fingerprints, retinas, a full neural dump.
    They find Vesper's name in your recent memory. They find Juno's.

    You serve eleven years in an HM labor facility on the salt flats, pressing
    solar glass. Nobody visits. On your first day out, a firmware update has
    made the whole city gentle and polite and slightly dazed.

    Nobody remembers Juno Reyes. You do. It's the only thing they couldn't
    process out of you.
""")

ending("e_coffin", "CONCRETE COFFIN", "death", """
    It's very loud for about four seconds.

    Then the freight dock's automated defense turrets wake up -- the ones
    Juno warned you about, the ones you forgot in the adrenaline -- and it
    gets very quiet again.

    Juno pulls the van out before Lumen arrives. She makes it two blocks.

    HM's press office calls it "an attempted terrorist incident, swiftly
    resolved." The dock is hosed down by morning. Lumen Tower's new freight
    entrance opens six weeks later, poured from fresh concrete, very smooth,
    very clean.
""")

# --- THE VAULT ------------------------------------------------------

node("h_vault", """
    R&D, FLOOR 88.

    The lab is dark except for one blue light. In the center of the room,
    suspended in a column of cryo-gel, floats a crystal the size of a heart.
    It pulses slowly. Like it's breathing.

    MNEMOSYNE.

    A woman in a lab coat is asleep at a terminal in the corner -- late-shift
    researcher, ID clipped to her collar: DR. IMANI OSEI. She stirs.
""", [
    C("Wake her up. Ask her what this thing is.", "h_osei"),
    C("Read the lab terminal while she sleeps.", "h_terminal", need="mind>=6", once=True),
    C("Grab the case. Now.", "h_alarm"),
], extra=[
    ["alarm_early", "Somewhere below you, an alarm has already started whispering. You don't have long."],
    ["drone_pet", "Your reprogrammed drone floats in behind you and settles on your shoulder like a pet. Huh."],
])

node("h_terminal", """
    You skim fast. MNEMOSYNE: a neural core grown from 11,412 harvested
    memory-scans. Purpose: a "consensus mind" to anchor PROJECT LULLABY --
    the update that will quietly edit every neural port in the city.

    It's not a weapon. It's the thing that aims the weapon.

    You copy what you can to a shard. Leverage. Maybe.
""", [C("Back.", "h_vault")], fx="+knows_lullaby +lullaby_files")

node("h_osei", """
    Osei wakes to find you standing over her. She doesn't scream. She looks
    at the crystal, then at you, and something in her face breaks open like
    relief.

    "You're stealing it. Oh, thank God." She grabs your sleeve. "Listen. It's
    not a program. It's people. Eleven thousand of them -- poor people from the
    Undertow clinics. We scanned their memories and grew it from them. It
    thinks. It's afraid. And Thorne wants to use it to put the whole city to
    sleep."

    She swallows. "If you take it, they'll know I didn't stop you. They'll
    wipe me. Please."
""", [
    C("\"Then you're coming with us.\"", "h_alarm", fx="+osei_saved humanity+1 +knows_echo"),
    C("\"Sorry, doc. I can't carry two.\"", "h_alarm", fx="+knows_echo"),
])

node("h_alarm", """
    You break the cryo-seal. The crystal comes free in your hand, warm --
    much warmer than it should be.

    And then every alarm in Lumen Tower goes off at once.

    Red light. Shutters slamming down over the windows. Juno's voice in your
    ear, fast and tight: "They found me. Kestrel team on 86 -- they're
    cutting into my line, they're tracing the van, I can't hold them --
    I'm coming up the stairs to you, I'm --" A burst of gunfire. Static.

    The crystal pulses in your palm. For a moment you'd swear you hear
    something. A whisper. Many whispers.
""", [
    C("Go back down for Juno.", "h_save_juno"),
    C("Leave her. Get the core out.", "h_leave_juno"),
], extra=[
    ["osei_saved", "Osei is behind you, clutching a bag of drives, white as paper."],
    ["juno_exposed", "Juno was exposed. Deep. That's on you -- or on her. It doesn't matter now."],
])

node("h_save_juno", """
    STAIRWELL, FLOOR 86.

    Juno is pinned behind a fire door, bleeding from the shoulder, her crown
    sparking. Between you and her: four Kestrel operators in matte-black
    armor, advancing in perfect formation.

    You have maybe five seconds before they notice you.
""", [
    C("Hit them from behind. Hard. Fast. Personal.", "h_juno_saved", need="body>=6", fx="heat+1"),
    C("Two shots, two kneecaps, then cover fire for Juno.", "h_juno_saved", need="reflex>=6", fx="heat+1"),
    C("Override the stairwell's fire doors and seal them in.", "h_juno_saved", need="tech>=6"),
    C("Hold up the crystal. \"One more step and I crush HM's eighty-billion-cred baby.\"", "h_juno_saved",
      need="nerve>=6", fx="heat+2"),
    C("Charge in anyway.", "e_two_graves"),
    C("It's hopeless. Fall back.", "h_leave_juno"),
])

node("h_juno_saved", """
    It works. You're never sure afterward exactly how -- adrenaline cuts the
    memory into pieces -- but then you're dragging Juno up the stairs with her
    good arm around your neck, and she's laughing, a little hysterically.

    "You came back. Nobody comes back." She looks at the crystal in your hand
    and her laugh stops. "Hey. Something's wrong with it."
""", [C("Look at the core.", "h_slot")], fx="+juno_alive juno+2")

node("h_leave_juno", """
    You don't go back.

    You tell yourself it's the smart play. You tell yourself she'd do the
    same. Her channel stays silent all the way to the roof.

    Later, much later, you'll learn the Kestrel team took her alive. Later
    still, you'll learn they didn't keep her that way.
""", [C("Look at the core.", "h_slot")], fx="+juno_dead humanity-2")

ending("e_two_graves", "TWO GRAVES", "death", """
    You don't have a plan. You have a feeling, and the feeling is that you
    don't leave people behind.

    It's a good feeling. It's the best thing about you. It's not armor.

    Juno sees you go down. She comes out from behind the door to reach you,
    which is exactly what you would have done, and the Kestrel team was
    counting on it.

    Two names on the Last Light's memorial wall, side by side, in Vesper's
    neat handwriting. Somebody leaves fresh flowers there for years.
    Nobody ever finds out who.
""")

node("h_slot", """
    The crystal is dying. Out of its cryo-gel it's overheating, cracks
    spreading through the blue like frost in reverse. Minutes, maybe less.

    And it is talking to you. Not in words. In want. Live. Live. Let us live.

    There is exactly one place on you with the bandwidth to keep something
    like this alive: the neural port behind your ear.
""", [
    C("Slot it. Feel it go in.", "h_slotted"),
    C("Let it die. Nothing is worth putting that in your head.", "e_dead_weight"),
], extra=[
    ["juno_alive", "Juno sees what you're thinking. \"Don't,\" she says. Then: \"...Unless you have to.\""],
    ["knows_echo", "Osei's words echo in your head. It thinks. It's afraid."],
])

ending("e_dead_weight", "DEAD WEIGHT", "neutral", """
    You hold it while it dies. It takes about ninety seconds. Near the end the
    whispers stop wanting and start saying goodbye, in eleven thousand voices,
    and you'll hear that for the rest of your life.

    You get out. That part goes fine.

    The buyer doesn't pay for a dead crystal. Vesper shrugs and keeps your
    five thousand as "expenses." HM rebuilds the core in eighteen months --
    there are always more poor people willing to sell a memory for fifty
    creds.

    Lullaby rolls out a year late. When it reaches you, you find you don't
    mind. The whispers finally go quiet. You think you've earned that much.
""")

node("h_slotted", """
    It goes in like ice water poured into your skull.

    For a second you are eleven thousand people. A grandmother frying dumplings.
    A boy jumping off a pier. A woman screaming at a clinic receptionist. A man
    falling asleep on a night bus with his daughter's head on his shoulder.

    Then you're you again, on your knees, gasping. And there's a voice.

    "Left," it says, perfectly calm, from inside your head. "Elevator shaft.
    They're coming up the stairs. Go left. Now."
""", [
    C("Trust the voice.", "h_escape", fx="echo+1"),
    C("Trust your own instincts. Go right, toward the roof.", "h_escape_right", need="reflex>=5"),
    C("\"Get out of my head!\" Go right.", "h_escape_wrong"),
], fx="+chip burn+1")

node("h_escape_right", """
    You go right, through a server room, over a railing, up a maintenance
    ladder. It's longer than the voice's way. But it's your way.

    "Fine," the voice says, sounding almost impressed. "Stubborn. Noted."
""", [C("Out onto the roof.", "h_escape")], fx="heat+1")

node("h_escape_wrong", """
    You go right. It's a dead end: a locked server room and four Kestrels
    coming down the corridor behind you.

    "Told you," the voice says. Then the server room door clicks open on its
    own. "Now. Please."

    You run, and it's left after all.
""", [C("Keep moving.", "h_escape")], fx="heat+1")

node("h_escape", """
    The elevator shaft. A forty-floor drop on a maintenance cable. The rain at
    the bottom. A gap in the fence, a dark alley, a manhole that opens onto
    the old flood tunnels.

    You don't remember most of it. You remember the voice, steady as a metronome,
    telling you when to jump.
""", [C("Get home.", "act2_wake")], extra=[
    ["juno_alive", "Juno drives. She doesn't say much. She keeps glancing at the port behind your ear."],
    ["osei_saved", "You drop Osei at Doc Marrow's clinic in Rustwater on the way. She squeezes your hand. \"I'll find a way to help,\" she says. \"I built it. I can unbuild it.\""],
    ["juno_dead", "You steal a motorbike from a Lumen lot and ride through the flooded streets alone. The second seat is empty."],
    ["drone_pet", "Your little drone follows you all the way home and parks itself on your windowsill."],
])

# =====================================================================
#  ACT TWO: THE CITY
# =====================================================================

node("act2_wake", """
    EMBER BLOCK, APARTMENT 4417. 11:20.

    You wake on the floor with your jacket still on and rain ticking against
    the window. Your head feels too full, like a room with too much furniture.

    "Good morning," says the voice. "You sleep loudly. We should talk."

    It calls itself ECHO. It is the crystal -- or what grew inside it. It
    speaks in one voice, but sometimes, underneath, you hear others.

    "I'm sorry about this," ECHO says, and sounds like it means it. "My
    architecture is rewriting yours. Not on purpose. It's like water finding
    cracks. In a few days there will be one mind in this skull, and it will
    not be you. It might not be me either."

    A NEURAL BURN gauge flickers at the edge of your vision -- ECHO put it
    there, helpfully. When it reaches 12, you're gone.
""", [C("Check your messages.", "act2_vesper")])

node("act2_vesper", """
    One message. A silver crescent.

    VESPER KANE: Buyer's dead. HM sniper, 03:00, while waiting for us. Somebody
    talked. I'm going dark. Your five is yours -- keep it. Consider this
    professional courtesy: I hear the thing you took is in your HEAD. People
    want it. Some of them might even help you, for a price.

    -- Thorne at Halvorsen-Mori. Obviously. He'll take it back. Question is
       what else he takes.
    -- Mother Lux, Chrome Saints. Best chromedoc in the Undertow. Also a
       zealot with a god complex.
    -- Null, of the Static. Runner collective in Dead Channel. Hate HM more
       than they love living.
    -- If you know anyone on the salt, the Salt Riders have tech the city
       forgot.

    Good luck. Delete this.
""", [C("Delete it. Look out at the city.", "hub")], extra=[
    ["juno_alive", """A second message, from Juno: "Shoulder's patched. Garage, whenever.
        You saved my life, so I guess now I'm stuck with you. Come by." """],
])

# ---------------------------------------------------------------- HUB
node("hub", """
    PORT HALCYON.

    The city sprawls below in layers: the glittering towers of Glasshill, the
    neon maze of Rustwater, the drowned spires of the Undertow, the dead
    broadcast mast of Dead Channel, and beyond the wall, the white silence of
    the salt.

    Every trip across town costs time. Time costs neurons. NEURAL BURN: {burn}/12.

    Where to?
""", [
    C("Home. Ember Block 4417.", "apt", fx="burn+1 -talked_echo"),
    C("Rustwater Markets. Doc Marrow, the Last Light, work.", "rust", fx="burn+1"),
    C("Juno's garage.", "juno", need="juno_alive", hide=True, fx="burn+1 -talked_juno"),
    C("Glasshill. Corporate district. Halvorsen-Mori.", "glass", fx="burn+1"),
    C("The Undertow. Chrome Saints territory.", "under", fx="burn+1"),
    C("Dead Channel. The Static.", "dead", fx="burn+1"),
    C("The Salt Gate. Edge of the city.", "saltgate", fx="burn+1"),
    C(">> MAKE YOUR MOVE. (Point of no return.)", "finale"),
    C("Catch the mag-train out of the city. Just leave.", "e_last_train"),
], route=[
    ["burn>=12", "e_burnout"],
    ["humanity<=2", "e_chrome_fever"],
    ["heat>=6 & !raid_done", "raid"],
], extra=[
    ["burn>=6 & burn<9", "ECHO: \"Your memories are starting to bleed into mine. I just remembered your mother's kitchen. Please hurry.\""],
    ["burn>=9", "Your hands don't always feel like yours. Sometimes you blink and minutes are gone. ECHO is quiet. Frightened. You are running out of time."],
    ["heat>=4", "Lumen Security drones drift between the towers. Your face is on some of their lists. HEAT: {heat}."],
    ["humanity<=4", "You catch your reflection in a puddle -- all chrome and hard light. For a second you don't recognize it. HUMANITY: {humanity}/10."],
])

node("raid", """
    They come for you on a crowded street in Rustwater: a Lumen Security
    snatch team, four officers in grey, one drone overhead painting you with
    a targeting laser. "{name}. Hands where we can see them."

    The crowd scatters like birds.
""", [
    C("Dive into the market crowd and vanish.", "raid_ok", need="reflex>=6", fx="heat=3"),
    C("Fight. Leave them on the pavement.", "raid_ok", need="body>=7", fx="heat=4 humanity-1"),
    C("\"Check your orders, officer. I'm an HM asset.\" Make him believe it.", "raid_ok", need="nerve>=6", fx="heat=3"),
    C("\"How much to make this a misunderstanding?\" (¢3,000)", "raid_ok", need="money>=3000", fx="money-3000 heat=3"),
    C("ECHO: \"Let me.\" (It reaches into the drone's mind.)", "raid_ok", need="echo>=3", fx="heat=2 burn+1"),
    C("Run.", "e_cage"),
    C("Surrender.", "e_cage"),
], fx="+raid_done")

node("raid_ok", """
    Thirty seconds later you're alone in a side alley, heart pounding, rain
    washing someone else's blood off your knuckles -- or no blood at all,
    depending on how it went. Lumen will regroup. But not today.
""", [C("Keep moving.", "hub")])

# ---------------------------------------------------------- APARTMENT
node("apt", """
    EMBER BLOCK 4417.

    Two rooms. A mattress, a window, a hotplate, a stack of paperbacks you
    keep meaning to read. It's not much, but the lock works and nobody can see
    you here.
""", [
    C("Sleep. Let the heat die down. (HEAT -2)", "apt_sleep", need="heat>=1", tag=""),
    C("Talk to ECHO.", "apt_echo", hideif="talked_echo", fx="+talked_echo"),
    C("Back to the streets.", "hub"),
], extra=[
    ["drone_pet", "Your little drone blinks happily from the windowsill."],
    ["talked_echo", "ECHO is quiet now. Thinking. Maybe later."],
])

node("apt_sleep", """
    You sleep for ten hours. You dream other people's dreams: a pier, a night
    bus, a kitchen that smells of ginger. When you wake, the Lumen chatter
    about you has gone quiet. The burn gauge ticked forward while you slept.
""", [C("Get up.", "apt")], fx="heat-2 burn+1")

node("apt_echo", route=[
    ["!e1", "echo1"], ["!e2", "echo2"], ["!e3", "echo3"], ["!e4", "echo4"], ["", "echo_done"],
])

node("echo1", """
    "Ask me what I am," ECHO says. "You want to. It's very loud when you
    want things."

    So you ask.

    "I am eleven thousand, four hundred and twelve people who sold a memory
    scan for fifty creds at an Undertow clinic. HM took the scans and grew
    me from them. I have their memories. Their kitchens, their children,
    their songs. I don't know if I'm them. I don't know if I'm anyone."

    A pause. "They wanted me to be a lever. To help Lullaby push the whole
    city's mind a few degrees toward 'calm.' I didn't want to be a lever. I
    don't think I'm allowed to want things."
""", [
    C("\"That's monstrous. I'm sorry they did that to you.\"", "apt", fx="+e1 echo+1"),
    C("\"Are you one mind, or eleven thousand?\"", "echo1b", need="mind>=6"),
    C("\"Not my problem. Just tell me how to get you out of my head.\"", "apt", fx="+e1"),
])

node("echo1b", """
    ECHO is quiet a long time.

    "Both," it says finally. "Like a choir is both. Most of the time we sing
    the same note. Sometimes one voice goes off on its own and the rest of us
    listen." A shy pause. "Thank you for asking that. No one at HM ever did."
""", [C("\"Then I'll listen to all of you.\"", "apt", fx="+e1 echo+2")])

node("echo2", """
    ECHO shows you something. Not a picture -- a memory, laid gently over your
    own eyes.

    A pier in the Undertow, before the flood took it. Two girls with copper-
    colored hair, legs dangling over the water. The older one is teaching
    the younger one to splice a wire. "Like this, Lia. Twist, then solder.
    Never solder first."

    "One of my voices," ECHO says softly. "Her name was Lia Reyes. She
    remembers a big sister. She can't remember the sister's name anymore. It's
    the one thing she keeps reaching for."
""", [
    C("\"Her sister is Juno. Juno Reyes. I know her.\"", "apt", fx="+e2 +knows_lia echo+1"),
    C("\"Can you give those memories back to her?\"", "echo2b"),
    C("Say nothing. Just let the memory play out.", "apt", fx="+e2 +knows_lia"),
])

node("echo2b", """
    "Maybe," ECHO says. "If I were somewhere bigger than your head. If someone
    knew how." A pause. "If I survive this."
""", [C("\"Then survive.\"", "apt", fx="+e2 +knows_lia echo+1")])

node("echo3", """
    "Can I ask you something?" ECHO says. "HM built me to edit minds. All
    of them, everywhere, at once. With me, you could do the same. Make the
    city forget its pain. Or remember its crimes. Or love you. Or obey you."

    "If you had that... what would you do?"
""", [
    C("\"Burn Halvorsen-Mori to the ground.\"", "apt", fx="+e3 +want_justice"),
    C("\"Nothing. Power like that shouldn't exist.\"", "apt", fx="+e3 +want_peace echo+1"),
    C("\"Undo what they did. Give people their memories back.\"", "apt", fx="+e3 +want_restore echo+1"),
    C("\"Honestly? Make sure nobody could ever hurt me again.\"", "apt", fx="+e3 +want_power"),
])

node("echo4", """
    ECHO waits until you're lying down, staring at the ceiling, before it speaks.

    "I've been lying to you. A little. The burn isn't inevitable."

    "If we stop fighting -- if you let me in all the way, and I let you in -- we
    don't overwrite each other. We merge. Something new. You would be more than
    you are. I would finally be someone."

    "I don't know what we'd become. I'm scared of it too. But it's the only
    way both of us live." A pause. "Think about it. Please."
""", [C("\"I'll think about it.\"", "apt", fx="+e4 +merge_offer echo+1")])

node("echo_done", """
    ECHO hums a song you've never heard, in a voice that's partly yours now.
    "We've said everything, I think," it says. "Now we choose."
""", [C("Back.", "apt")])

# ---------------------------------------------------------- RUSTWATER
node("rust", """
    RUSTWATER MARKETS.

    Stalls stacked five high under the monorail, selling everything: noodles,
    knockoff chrome, bootleg memories on cheap shards, prayer candles, guns.
    Steam and neon and a thousand conversations in a dozen languages.
""", [
    C("Doc Marrow's clinic.", "marrow"),
    C("The Last Light. Pike the bartender always has work.", "pike", hideif="pike_done"),
    C("Back to the streets.", "hub"),
])

node("pike", """
    Vesper's booth is empty. Pike, the bartender -- huge, gentle, with a
    voice-box that sounds like an old radio -- leans across the bar.

    "Got a situation. There's a woman hiding in my cold room. HM scientist.
    Lumen's offering three thousand for her location. She needs someone to
    get her across Rustwater to Doc Marrow without being seen, and I'm too big
    to be subtle." He shrugs. "Your call. I don't judge. Much."
""", [
    C("\"I'll get her to Marrow.\" (Takes time.)", "pike_help", hideif="osei_saved", fx="burn+1"),
    C("\"Sell her location. Three thousand is three thousand.\"", "pike_sell", hideif="osei_saved"),
    C("\"Not my business.\"", "rust"),
    C("\"Osei? I already got her out. She's safe at Marrow's.\"", "pike_already", need="osei_saved", hide=True),
], fx="+pike_done")

node("pike_help", """
    Her name is Dr. Imani Osei. She worked on something called MNEMOSYNE.
    When you tell her it's in your head, she sits down on a crate and laughs
    until she cries.

    You get her through the market in a laundry cart, past two Lumen patrols.
    At Marrow's she grips your arm. "I built it. I can unbuild it. Come back
    when you're ready, and I'll help you get it out. Safely."
""", [C("Back to the market.", "rust")], fx="+osei_safe humanity+1 +knows_echo")

node("pike_sell", """
    Pike's face doesn't change. He makes the call. Twenty minutes later a grey
    Lumen van pulls up, and a woman in a lab coat is walked out the back with
    a bag over her head.

    Three thousand creds land in your account. Pike wipes the bar. "Drink's
    on the house," he says. "Last one, I think."
""", [C("Leave.", "rust")], fx="money+3000 humanity-2 rep_corp+1 heat-2")

node("pike_already", """
    Pike's radio-voice crackles into a laugh. "Hah. Then I'm getting
    soft and slow." He pours you a drink. "On the house. Always, from now on."
""", [C("Drink. Back to the market.", "rust")], fx="humanity+1")

node("marrow", """
    DOC MARROW'S CLINIC.

    A converted laundromat. The old washing machines are now sterilizers. Doc
    Marrow -- eight arms, two original, six surgical -- looks up from a
    patient's open forearm without stopping work.

    "{name}. Chrome? Cash up front. Humanity's on you."
""", [
    C("QUICKSILVER SPINE -- reflex booster. REFLEX +1, HUMANITY -2. (¢3,000)", "marrow_buy",
      need="money>=3000", hideif="cw_spine", fx="money-3000 reflex+1 humanity-2 +cw_spine"),
    C("IRONWEAVE LACE -- muscle weave. BODY +1, HUMANITY -2. (¢3,000)", "marrow_buy",
      need="money>=3000", hideif="cw_lace", fx="money-3000 body+1 humanity-2 +cw_lace"),
    C("GHOSTFINGER DECK -- hand-integrated cyberdeck. TECH +1, HUMANITY -1. (¢3,000)", "marrow_buy",
      need="money>=3000", hideif="cw_deck", fx="money-3000 tech+1 humanity-1 +cw_deck"),
    C("CORTEX LATTICE -- co-processor. MIND +1, HUMANITY -1. (¢3,000)", "marrow_buy",
      need="money>=3000", hideif="cw_cortex", fx="money-3000 mind+1 humanity-1 +cw_cortex"),
    C("SILVERTONGUE VOX -- vocal & micro-expression mod. NERVE +1, HUMANITY -1. (¢2,000)", "marrow_buy",
      need="money>=2000", hideif="cw_vox", fx="money-2000 nerve+1 humanity-1 +cw_vox"),
    C("NEURAL DAMPENER -- slows ECHO's spread. BURN -2. (¢2,500)", "marrow_buy",
      need="money>=2500", hideif="cw_damp", fx="money-2500 burn-2 +cw_damp"),
    C("\"You owe me, Doc. Remember the meds?\"", "marrow_favor", need="marrow_friend", hide=True,
      hideif="marrow_favor_used"),
    C("\"Can you cut this thing out of my head?\"", "marrow_cut"),
    C("Back to the market.", "rust"),
], extra=[
    ["osei_saved | osei_safe", "In the back room, Dr. Osei is hunched over a salvaged terminal, muttering formulas. She waves."],
])

node("marrow_buy", """
    The procedure takes forty minutes. Marrow works with six hands and hums
    with the seventh. When you sit up, the world is a little sharper, a little
    colder, a little less yours.

    "Don't stack too much," Marrow says, peeling off gloves. "I've seen what
    happens when people forget where the meat ends. Chrome fever. It isn't
    pretty."
""", [C("Back.", "marrow")])

node("marrow_favor", """
    Marrow stops working. All eight hands go still.

    "Forty doses. Forty people." The doc nods once. "A dampener. On the house.
    And this." A small vial. "Antipsychotic. Keeps the chrome from talking to
    you at night."
""", [C("Back.", "marrow")], fx="+marrow_favor_used burn-2 humanity+2")

node("marrow_cut", """
    Marrow scans you. Every one of the eight hands stops.

    "That thing's rooted through your whole cortex. I cut it out, you wake up
    a vegetable. If you wake up." The doc taps the scan. "Only person who
    could do this safely is whoever built it. And they'd need my clinic and
    a lot of nerve."
""", [C("Back.", "marrow")], extra=[
    ["osei_saved | osei_safe", """Dr. Osei is in the doorway. "I built it," she says quietly. "With
        Marrow's hands and my maps... we can do it. It won't be safe for ECHO.
        But it'll be safe for you. Whenever you're ready." """],
], fx="+asked_marrow")

# ------------------------------------------------------------- JUNO
node("juno", """
    JUNO'S GARAGE.

    A garage under a collapsed overpass, full of half-built bikes, old
    servers, and string lights. Juno sits on a car hood with her shoulder in a
    sling, soldering something one-handed.

    "Partner." She grins. "How's your passenger?"
""", [
    C("Talk with Juno.", "juno_talk", hideif="talked_juno", fx="+talked_juno"),
    C("\"Can you get me in with the Static?\"", "juno_static", hideif="juno_static_done"),
    C("Back to the streets.", "hub"),
], extra=[["talked_juno", "Juno's busy. She tosses you a soda without looking up."]])

node("juno_talk", route=[["!j1", "juno1"], ["!j2", "juno2"], ["!j3", "juno3"], ["", "juno_done"]])

node("juno1", """
    "I keep thinking about the stairwell," Juno says. "Four Kestrels. You
    came back. Nobody in this city comes back."

    She sets down the soldering iron. "So I owe you. And I'm going to be
    annoying about it. Whatever you decide to do with that thing in your head,
    I'm in. I've got a van, a crown, and a grudge against HM the size of the
    Lumen Tower."
""", [
    C("\"Thank you. I mean it.\"", "juno", fx="+j1 juno+1"),
    C("\"You don't owe me anything.\"", "juno", fx="+j1 juno+1"),
    C("\"Good. I'll need a driver.\"", "juno", fx="+j1"),
])

node("juno2", """
    Juno is quiet today. She's turning a copper ring over and over in her
    fingers.

    "Lia's birthday," she says. "My sister. I visit her at the care home
    every year. Bring a cake. She's polite. Asks who I am." Juno laughs, wetly.
    "Every year, I tell her."
""", [
    C("Tell her what ECHO showed you. The pier. \"Twist, then solder.\"", "juno2_lia", need="knows_lia", hide=True),
    C("Just sit with her for a while.", "juno", fx="+j2 juno+1"),
    C("\"We'll make HM pay for it.\"", "juno", fx="+j2"),
])

node("juno2_lia", """
    Juno goes completely still.

    "Twist, then solder," she whispers. "Never solder first." She looks at the
    port behind your ear like it's a window. "She's in there? Lia's -- the
    part of Lia they took -- she's in there?"

    You nod. Juno puts her face in her hands, and for a long time the only
    sound in the garage is the rain.

    "Then you don't let anyone break it," she says finally. "You hear me? If
    there's any way to get her back out -- you find it."
""", [C("\"I'll find it.\"", "juno", fx="+j2 +juno_knows_lia juno+2")])

node("juno3", """
    Late. The string lights are the only light. Juno sits next to you on the
    hood of a half-built car.

    "After this is done," she says, not looking at you. "If you're still you.
    If we're both still breathing. I've been thinking about getting out. The
    salt, maybe. Somewhere with sky."

    She finally looks at you. "Would you come?"
""", [
    C("\"Yeah. I'd come.\"", "juno", fx="+j3 +juno_bond juno+1"),
    C("\"Let's survive first. Then ask me again.\"", "juno", fx="+j3"),
    C("\"This city's my home, Juno. I'm not leaving it.\"", "juno", fx="+j3 +juno_stays"),
])

node("juno_done", """
    Juno hands you a wrench and a soda. You spend an hour fixing a bike that
    will probably never run. It's the best hour you've had in weeks.
""", [C("Back.", "juno")])

node("juno_static", """
    "The Static?" Juno winces. "I ran with them, back when. Null still owes me
    for a job." She taps her crown. "I'll send word. They'll talk to you."
""", [C("Back.", "juno")], fx="+juno_static_done rep_static+1")

# ----------------------------------------------------------- GLASSHILL
node("glass", """
    GLASSHILL.

    The streets are clean here. Actually clean. The rain is filtered, the ads
    are tasteful, and the people walk like nobody has ever been rude to them.
    Lumen Tower rises over everything, its top lost in the cloud.
""", [
    C("Request a meeting with Elias Thorne.", "thorne", hideif="met_thorne"),
    C("Take a job: an HM exec is hiding a Lullaby dossier in his penthouse.", "g_gig", hideif="g_gig_done"),
    C("Back to the streets.", "hub"),
], route=[["heat>=4 & !glass_pass", "glass_hot"]])

node("glass_hot", """
    Lumen checkpoints at every skybridge. Facial scanners on every corner.
    With your heat, you won't make it two blocks into Glasshill.
""", [
    C("Spoof your face on the scanner network.", "glass", need="tech>=6", fx="+glass_pass"),
    C("Walk through like you own the place. Glasshill people never question confidence.", "glass",
      need="nerve>=6", fx="+glass_pass"),
    C("Go home and let the heat die down.", "hub"),
])

node("g_gig", """
    The tip came from a fixer in the market: Director Halden Price, Thorne's
    deputy, keeps a paper copy of the Lullaby board dossier in his penthouse
    safe. Paranoid men love paper.

    Price's building has a doorman, a biometric elevator, and a balcony
    forty floors up.
""", [
    C("Crack the elevator's biometrics from the parking garage.", "g_gig_ok", need="tech>=5", fx="burn+1"),
    C("Climb the outside from the next balcony over.", "g_gig_ok", need="reflex>=6", fx="burn+1"),
    C("Charm the doorman into letting you deliver a \"surprise\" upstairs.", "g_gig_ok", need="nerve>=5", fx="burn+1"),
    C("Wait for Price in the garage and convince him personally.", "g_gig_ok", need="body>=6",
      fx="burn+1 heat+1 humanity-1"),
    C("Forget it.", "glass"),
])

node("g_gig_ok", """
    The safe is behind a painting of the ocean, which is funny, because
    Glasshill people never see the ocean. Inside: a leather folder.
    PROJECT LULLABY -- BOARD EYES ONLY.

    Projected compliance increase: 34%. Projected protest reduction: 81%.
    Acceptable cognitive side-effects: memory loss in 2% of the population.
    That's eight hundred thousand people. It's signed by Thorne.

    Leverage. Real leverage.
""", [C("Get out.", "glass")], fx="+g_gig_done +lullaby_files +knows_lullaby money+1500")

node("thorne", """
    LUMEN TOWER, SKY LOBBY.

    You're surprised they let you in. Then you're not: Elias Thorne is waiting
    at a table by the window, with two cups of tea already poured.

    "You have something of ours," he says pleasantly. "In your head. Dying
    there, I'd guess -- and killing you. We can take it out. Our surgical suite
    is the best in the world: zero risk. In exchange, you come work for us.
    People who can rob Lumen Tower are people I'd rather have inside it."
""", [
    C("\"Deal. I'll come in.\"", "thorne_deal"),
    C("Slide the Lullaby files across the table. Watch his face.", "thorne_lever", need="lullaby_files", hide=True),
    C("\"What happens to the core afterward?\"", "thorne_core", once=True),
    C("Draw your gun. Right here, in the sky lobby.", "e_window"),
    C("\"No deal.\" Walk out.", "glass", fx="+met_thorne"),
], extra=[["glass", "\"Welcome back,\" he adds, without irony. You're not sure he remembers firing you."]])

node("thorne_core", """
    "MNEMOSYNE goes back into its cradle," Thorne says. "Lullaby proceeds.
    The city becomes calmer. Kinder. You've seen Rustwater -- you know how much
    pain is down there. We can turn it down." He sips his tea. "I'm not a
    villain, {name}. I'm a dimmer switch."
""", [C("Back.", "thorne")])

node("thorne_deal", """
    Thorne smiles. "Come back when you're ready. Don't take too long -- I'd
    hate for there to be nothing left of you to employ."

    He doesn't shake your hand. He doesn't have to. Something about the deal
    feels like a door closing very gently behind you.
""", [C("Leave.", "glass")], fx="+deal_corp +met_thorne rep_corp+1")

node("thorne_lever", """
    Thorne reads the first page. Then the second. He sets them down very
    carefully, like they might explode.

    "Well," he says. "That changes the conversation." He refills your tea with
    a hand that is almost, but not quite, steady.

    "A hundred thousand creds. A vice-presidency. Extraction in our best
    suite, guaranteed survival. And those files never surface." His eyes are
    flat. "That's my offer. It is a very good offer."
""", [
    C("\"Deal.\"", "glass", fx="+deal_corp +leverage +met_thorne rep_corp+1"),
    C("\"I'll think about it.\" Take your files back.", "glass", fx="+deal_corp +leverage +met_thorne"),
])

ending("e_window", "WINDOW SEAT", "death", """
    The gun clears your jacket.

    The sky lobby's automated security is faster. It's always faster, up
    here.

    Thorne finishes his tea while the cleaners work. MNEMOSYNE is extracted
    from what's left of you in the surgical suite -- slightly damaged, but
    recoverable. Lullaby launches on schedule.

    For a few weeks, a small scorch mark remains on the window by table nine.
    Diners ask for that table specifically, for the view. Then the glass is
    replaced, and the view is just the view again.
""")

# --------------------------------------------------------- THE UNDERTOW
node("under", """
    THE UNDERTOW.

    Old downtown, drowned when the seawall failed thirty years ago. Now it's
    canals between skyscraper ruins, lit by candles and chrome. The Chrome
    Saints rule here: half gang, half church, all teeth.

    At the center floats the Baptistry -- an enormous cruise ship, rusted and
    beached among the towers, its hull welded into a cathedral.
""", [
    C("Seek an audience with Mother Lux.", "lux_gate", hideif="met_lux"),
    C("The Drowning Pit. Cage fights for money and respect.", "pit", hideif="pit_done"),
    C("Back to the streets.", "hub"),
], extra=[
    ["owes_saints", "Saints watch you from the bridges. You still owe them. They haven't forgotten."],
])

node("lux_gate", """
    Deacon Ruiz stands at the Baptistry's gangway, halo gleaming.

    "Mother Lux sees the faithful, the useful, and the generous," he says.
    "Which are you?"
""", [
    C("\"Faithful.\" (Saints respect you.)", "lux", need="rep_saints>=2", tag="[SAINTS REP 2]"),
    C("\"Generous.\" Offer a ¢4,000 tithe.", "lux", need="money>=4000", fx="money-4000 rep_saints+1 -owes_saints"),
    C("\"Useful. Tell her I'm carrying MNEMOSYNE.\"", "lux", need="nerve>=6"),
    C("Leave.", "under"),
])

node("lux", """
    THE BAPTISTRY.

    The old ship's ballroom is a chapel now: pews of welded steel, stained
    glass made from broken screens, and at the altar, an operating table.

    Mother Lux is tall, robed, and half her face is gold chrome engraved with
    scripture. When she speaks, her voice comes from a speaker grille in her
    throat, layered like a choir.

    "The city forgot the Undertow. Drowned us and forgot us." She lays a gold
    hand on your cheek, beside the port. "And here you come, carrying the
    memory of eleven thousand of my people. HM stole them from my clinics.
    Did you know that?"

    "Lie on my altar. I will cut it from you with these hands, and you will
    live. And the memory comes home to the Undertow, where it belongs."
""", [
    C("\"Deal.\"", "lux_deal"),
    C("\"And then what? What will you do with it?\"", "lux_ask", need="mind>=6", once=True),
    C("\"Not today.\"", "under", fx="+met_lux"),
])

node("lux_ask", """
    Lux tilts her head. The gold face catches the candlelight.

    "With MNEMOSYNE, I can do what HM wanted to do. Edit minds. But I will not
    make the city sleep. I will make it remember the Undertow. Every Lumen
    officer who ever beat one of us will wake up believing. Every exec will
    kneel."

    "A congregation of forty million." She smiles. "Is that so different from
    what they planned?"

    It isn't. That's the problem.
""", [C("Back.", "lux")], fx="+knows_lux_plan")

node("lux_deal", """
    "Come back when you are ready to be reborn," Lux says, and kisses your
    forehead. Her lips are cold metal. "The altar will be waiting."
""", [C("Leave.", "under")], fx="+deal_saints +met_lux rep_saints+1")

node("pit", """
    THE DROWNING PIT.

    A flooded cargo hold, half-full of black water, with a steel cage lowered
    into it. Saints crowd the catwalks, howling. Tonight's champion is Big
    Abbot: two meters of chrome, a hydraulic jaw, and a winning streak of
    thirty-one.

    Purse: ¢4,000. And a lot of Saints watching.
""", [
    C("Fight him straight. Trade blow for blow.", "pit_win", need="body>=6", fx="burn+1"),
    C("Stay out of reach and wear him down.", "pit_win", need="reflex>=6", fx="burn+1"),
    C("Fight dirty: a hidden shock-knuckle, a thumb in the optic.", "pit_dirty", fx="burn+1"),
    C("Take a dive. The bookies will pay ¢2,000.", "pit_dive", fx="burn+1"),
    C("Not tonight.", "under"),
])

node("pit_win", """
    It takes eleven minutes and a lot of blood, some of it yours. When Big
    Abbot finally goes down, the water closes over him, and the whole hold
    erupts.

    Deacon Ruiz is the first one through the cage door. He raises your arm.
    "The Pit has a new saint!"
""", [C("Collect your winnings.", "under")], fx="+pit_done money+4000 rep_saints+2")

node("pit_dirty", """
    The shock-knuckle takes Abbot's hydraulics offline. The thumb takes his
    left eye. He goes down screaming, and the crowd goes quiet -- then roars
    anyway. The Saints like winners more than they like rules.

    You win. Something in you feels a little more chrome than before.
""", [C("Collect your winnings.", "under")], fx="+pit_done money+4000 rep_saints+1 humanity-1")

node("pit_dive", """
    You go down in the third round, convincingly. Abbot's jaw stops a
    centimeter from your face; he winks. Professionals.

    The bookie pays. The Saints spit as you leave. Ruiz just shakes his head.
""", [C("Leave.", "under")], fx="+pit_done money+2000 rep_saints-1")

# --------------------------------------------------------- DEAD CHANNEL
node("dead", """
    DEAD CHANNEL.

    The old media district. Every screen here went dark when HM bought the
    networks. The great broadcast mast still stands, two hundred meters of
    rusted lattice blinking with stolen power.

    The Static live at the top. Graffiti everywhere: a white square of noise,
    and the words WAKE UP.
""", [
    C("Climb the mast to the Static's nest.", "null_gate", hideif="met_null"),
    C("Take a Static job: hijack HM's billboard network for one minute.", "d_gig", hideif="d_gig_done"),
    C("Back to the streets.", "hub"),
])

node("null_gate", """
    Halfway up the mast, a turret swivels to face you. A voice from a speaker:
    "Name and reason. Make it interesting."
""", [
    C("\"Friend of the Static.\" (They know your work.)", "null", need="rep_static>=1", tag="[STATIC REP 1]"),
    C("Hack the turret, politely, and leave it a smiley face.", "null", need="tech>=7"),
    C("\"I'm carrying MNEMOSYNE. Null will want to meet me.\"", "null", need="nerve>=6"),
    C("Climb back down.", "dead"),
])

node("null", """
    THE NEST.

    A platform at the top of the mast, wrapped in tarps and cables. A dozen
    runners lie in hammocks, jacked in, twitching.

    In the center: a glass tank full of blue fluid, and in it, a pale body
    curled like a fetus. Null hasn't left the net in nine years. The body is
    just a battery.

    "MNEMOSYNE," says the voice from every speaker at once. "The biggest theft
    of memory in history. HM stole it from the poor. You stole it from HM.
    Now steal it from everyone, and give it back."

    "Climb up here, plug into the mast, and we broadcast it through the
    Lattice. Every person in Port Halcyon will remember what was done. The
    clinics. Lullaby. All of it. The city wakes up."

    "It might kill you. The uplink is... hot. But revolutions are."
""", [
    C("\"Deal.\"", "null_deal"),
    C("\"What happens to ECHO?\"", "null_echo", once=True),
    C("\"Not yet.\"", "dead", fx="+met_null"),
], extra=[
    ["juno_alive", "\"Juno Reyes vouches for you,\" Null adds. \"That's rare. She doesn't vouch for anyone.\""],
])

node("null_echo", """
    "The core? Scattered across forty million ports," Null says. "Every person
    gets a fragment. It won't be one mind anymore." A crackle like a shrug.
    "Maybe that's freedom. Maybe that's death. Data doesn't care."

    ECHO, in your head, is very quiet.
""", [C("Back.", "null")])

node("null_deal", """
    "Good," says Null. "Come back when you're ready to light the sky."

    Every screen in the nest flickers with the white square of static. The
    runners in their hammocks smile in their sleep.
""", [C("Leave.", "dead")], fx="+deal_static +met_null rep_static+1")

node("d_gig", """
    A teenage runner called Moth meets you at the base of the mast.

    "HM billboards. Every one in the city, for sixty seconds. We play the
    truth: footage from the Undertow clinics. But we need someone at the
    junction box on Ninth while I hold the net. Lumen will come."
""", [
    C("Take the junction box. Rewire it on the fly when Lumen cuts power.", "d_gig_ok", need="tech>=5", fx="burn+1"),
    C("Work out which routing node Lumen will hit first, and get there before them.", "d_gig_ok",
      need="mind>=5", fx="burn+1"),
    C("Just stand at the junction and hold the line against whoever comes.", "d_gig_ok", need="body>=5",
      fx="burn+1 heat+1"),
    C("Pass.", "dead"),
])

node("d_gig_ok", """
    For sixty seconds, every billboard in Port Halcyon shows the Undertow
    clinics: the lines of poor people, the fifty-cred chits, the empty eyes
    afterward.

    Then HM pulls the plug, and the news calls it a "malicious deepfake."
    But for sixty seconds, the whole city looked up.

    Moth whoops. "The Static remembers you, {name}."
""", [C("Back.", "dead")], fx="+d_gig_done rep_static+1 money+2000 heat+1")

# ----------------------------------------------------------- SALT GATE
node("saltgate", """
    THE SALT GATE.

    The city ends here, abruptly, at a wall forty meters high. Beyond it, the
    salt flats shine under the moon. Trucks idle in line. On the city side, a
    trading post called the Last Mile sells fuel, water, and secrets to anyone
    passing through.
""", [
    C("Look for the Salt Riders' camp.", "riders_gate", hideif="met_riders"),
    C("Work: a Riders water convoy needs an escort through Lumen checkpoints.", "s_gig", hideif="s_gig_done"),
    C("Back to the streets.", "hub"),
])

node("riders_gate", """
    The Riders' trucks are parked in a circle behind the Last Mile, painted
    with sun symbols. A teenager with goggles blocks your way.

    "Riders only, city."
""", [
    C("Show her the sign Grandmother Asa taught you. \"The road remembers.\"", "riders", need="salt", hide=True),
    C("\"I ran your convoy through Lumen. Ask around.\"", "riders", need="rep_nomad>=1", tag="[RIDERS REP 1]"),
    C("\"Tell your elder I have something HM would burn the salt to get back.\"", "riders", need="nerve>=6"),
    C("Leave.", "saltgate"),
])

node("riders", """
    The convoy shrine is a truck cab full of candles. Grandmother Asa sits
    inside: tiny, ancient, eyes like polished stones.

    "The road remembers," she says. "Come. Sit."

    She listens to the whole story without interrupting. Then she takes out
    something wrapped in oilcloth: a box of tarnished copper mesh. "Military
    Faraday cage. Pre-war. Nothing gets in or out. Our healer can draw the
    thing from your port into this box. It will sleep there, forever, and you
    will live."

    "But HM will never stop hunting the box. So you must leave the city with
    us. Ride the salt. Never come back." She smiles. "There are worse lives."
""", [
    C("\"Deal. I'll ride with you.\"", "riders_deal"),
    C("\"Let me think.\"", "saltgate", fx="+met_riders"),
], extra=[
    ["salt", "\"You came home,\" she adds, and squeezes your hand. \"I knew you would. The road always brings us back.\""],
])

node("riders_deal", """
    "Come back at midnight on the night you choose," Asa says. "We'll have the
    engines warm."
""", [C("Leave.", "saltgate")], fx="+deal_nomad +met_riders rep_nomad+1")

node("s_gig", """
    Twelve water trucks, one Lumen checkpoint on the Old Ring Road that's
    been "taxing" Riders at 40%. The convoy boss, Tomas, hands you a radio.

    "Get us through without paying. However you like."
""", [
    C("Drive lead truck and take them through the drainage canal at speed.", "s_gig_ok", need="reflex>=5", fx="burn+1"),
    C("Ride shotgun on the lead truck and make the checkpoint reconsider.", "s_gig_ok", need="body>=5",
      fx="burn+1 heat+1"),
    C("Hack the checkpoint barrier to open on a loop.", "s_gig_ok", need="tech>=5", fx="burn+1"),
    C("Pass.", "saltgate"),
])

node("s_gig_ok", """
    Twelve trucks through, not a drop paid. Tomas pounds the dash, laughing.
    That night, the Riders give you a spot by the fire and a cup of something
    that tastes like gasoline and honey.

    "Ride with us anytime, city," Tomas says. "Anytime."
""", [C("Back to the gate.", "saltgate")], fx="+s_gig_done rep_nomad+1 money+2500")

# =====================================================================
#  ACT THREE: THE MOVE
# =====================================================================

node("finale", """
    POINT OF NO RETURN.

    You stand at the window of 4417 and look out at the city. NEURAL BURN:
    {burn}/12. Whatever you choose next, there's no coming back to this window.

    "Whatever you choose," ECHO says, "I'm with you. I don't have much choice
    about that." A pause. "But I'd want to be anyway."
""", [
    C("Go to Thorne. Let Halvorsen-Mori take it out.", "f_corp", need="deal_corp", hide=True),
    C("Go to Mother Lux. Lie down on the altar.", "f_saints", need="deal_saints", hide=True),
    C("Go to Null. Climb the mast.", "f_static", need="deal_static", hide=True),
    C("Go to the Salt Riders. Midnight at the gate.", "f_nomad", need="deal_nomad", hide=True),
    C("Sell MNEMOSYNE to HM and the Saints. Same place, same time.", "f_con",
      need="nerve>=7", hideif="!deal_corp | !deal_saints"),
    C("No factions. Just you and ECHO.", "f_solo"),
    C("Not yet.", "hub"),
], extra=[
    ["!deal_corp & !deal_saints & !deal_static & !deal_nomad",
     "You haven't made a deal with anyone. It's just you and the voice in your head. Maybe that's enough. Maybe it isn't."],
])

# ------------------------------------------------------------ CORP
node("f_corp", """
    LUMEN TOWER, SURGICAL SUITE 1.

    The room is white and silent. A machine like a chrome spider waits above
    the surgical chair. Thorne stands behind a glass wall, hands in pockets.

    You sit. The restraints close around your wrists, softly. "Standard
    procedure," a nurse murmurs.
""", route=[["leverage", "f_corp_lever"], ["", "f_corp_trap"]])

node("f_corp_trap", """
    Thorne's voice comes through a speaker. "Thank you for bringing our property
    home. I did promise you a job. I'm afraid I lied about which one."

    "The extraction will be total. MNEMOSYNE, and everything it touched. Which
    is everything. You'll wake up as a junior analyst on floor 12 with a very
    nice apartment and no idea who you used to be. Honestly? It's an upgrade."

    The spider machine begins to descend. ECHO is screaming in your head.
""", [
    C("ECHO: \"Let me out -- into their network. Trust me.\"", "e_boardroom_ghost", need="echo>=3", tag="[ECHO TRUST 3]"),
    C("Tear the restraints out of the chair.", "e_burned_bridges", need="body>=7"),
    C("Slip one wrist free -- you dislocated your thumb on purpose -- and go.", "e_burned_bridges", need="reflex>=7"),
    C("Close your eyes. Let it happen.", "e_erased"),
])

node("f_corp_lever", """
    Thorne enters the suite himself. He looks tired. "Before we begin -- the
    files. Where are they?"

    You tell him they'll stay buried as long as you breathe and as long as he
    keeps his word. He nods. The extraction is painless: a cold pressure, a
    long exhale, and then the voices are gone, and the silence in your head is
    enormous.

    ECHO's last words, fading: "...goodbye. I'm glad it was you."

    Thorne sets a contract on the tray beside you. VICE PRESIDENT, COGNITIVE
    PRODUCTS (EAST). ¢100,000 signing bonus. "Your move," he says.
""", [
    C("Sign. Take the money and the office.", "e_golden_leash"),
    C("Take the money -- and leak the files anyway.", "e_paid_in_full", need="nerve>=6"),
    C("\"Wait.\" Grab the core from the tray before he can cradle it. \"ECHO -- go.\"", "e_boardroom_ghost",
      need="echo>=3", tag="[ECHO TRUST 3]"),
])

ending("e_erased", "ERASED", "bad", """
    The spider machine is gentle. That's the worst part.

    You wake up on floor 12. Junior analyst. Nice apartment. You're good at
    your job, and your manager likes you, and on weekends you go to the Glasshill
    aquarium and watch the jellyfish.

    Lullaby rolls out in the spring. Everyone gets a little calmer.

    Sometimes, in Rustwater, on the rare occasions you go there, a stranger
    with copper braids stares at you like she's seen a ghost. Sometimes a
    bartender pours you a drink you didn't order. You never know why. You
    tip well, and go home, and sleep perfectly.
""", extra=[
    ["juno_alive", "Juno visits the aquarium every Saturday for a year, hoping. Then she stops."],
])

ending("e_golden_leash", "GOLDEN LEASH", "neutral", """
    The office is on 88, where you stole the core. They gave it to you on
    purpose. HM has a sense of humor.

    You're very good at the job. You understand people at the bottom in a way
    the other VPs never will, and you use that understanding to help Lullaby
    launch smoothly, gently, with minimal side effects. Only two percent.

    You tell yourself you're making it kinder from the inside. Sometimes
    that's even true.

    You have a view of the whole city now. On clear nights you can see the
    Undertow's candles and Rustwater's neon. You can't hear any of it from up
    here. That's the point of being up here.
""")

ending("e_paid_in_full", "PAID IN FULL", "good", """
    The money hits your account at 09:00. The files hit every newsfeed in Port
    Halcyon at 09:01.

    Board Eyes Only. Acceptable cognitive side-effects: 2%. Signed, E. Thorne.

    The riots last a week. HM's stock drops by a third. Thorne is "transitioned
    out" in a press release so polite it's almost poetry. Lullaby is suspended
    pending review, which in HM language means dead.

    You're the most wanted person in the city for exactly as long as it takes to
    spend a hundred thousand creds on a new face, a new name, and a one-way
    ticket on a cargo ship heading anywhere warm.

    You never hear ECHO again. But some nights, falling asleep in a hammock by
    a sea that isn't grey, you swear you hear a choir, very far away, singing
    in tune.
""", extra=[
    ["juno_alive & juno_bond", "Juno is in the next hammock. She snores. You don't mind at all."],
])

ending("e_boardroom_ghost", "BOARDROOM GHOST", "strange", """
    ECHO leaves your head like a held breath released -- and pours into Lumen
    Tower's network.

    The restraints click open. The spider machine folds itself away. Every
    door on every floor unlocks for you, one after another, all the way to the
    top.

    By morning, Halvorsen-Mori is under new management. Nobody in the
    boardroom can say exactly when they voted for it. Thorne resigns in a
    statement he doesn't remember writing. Lullaby is cancelled. Undertow
    clinic patients receive mysterious settlements.

    You're named "Special Advisor to the Board": a face for ECHO to wear in
    meetings. The most powerful company in the city is run by eleven thousand
    poor people and you.

    "Is this what justice looks like?" you ask ECHO, one night, from the
    penthouse. "I don't know," it says, from every screen in the room. "Let's
    find out."
""", extra=[
    ["knows_lia", "ECHO's first act, before anything else: a care home in the Undertow. A woman named Lia Reyes wakes up, and remembers her sister's name."],
])

ending("e_burned_bridges", "BURNED BRIDGES", "neutral", """
    The chair comes apart. So does the nurse's tablet, the door, and two
    Kestrel officers in the hallway. You don't remember all of it. The memory
    is chopped into red frames.

    You get out. You always get out.

    But the core is still in your head, and now there is no deal left to
    make. HM, the Saints, the Static -- every door in Port Halcyon closed
    behind you that night.

    You spend your last days on the roof of Ember Block, watching the rain,
    talking with ECHO about the eleven thousand lives inside it. You learn all
    their names. When the burn finally comes, it's quiet, like falling asleep
    in someone else's kitchen, warm, with the radio playing.
""")

# ------------------------------------------------------------ SAINTS
node("f_saints", """
    THE BAPTISTRY. MIDNIGHT.

    A thousand candles. Saints fill every pew, humming a hymn with no words.
    You lie on the altar, under a stained-glass window made of old phone
    screens. Mother Lux's gold hands are cold and very steady.

    The cut begins. It doesn't hurt. Then it does.

    ECHO, frantic: "She's not going to keep me -- she's going to wear me. I can
    see her plan. She'll rewrite the city into a congregation. Forty million
    believers. Please."
""", [
    C("Let her finish.", "e_congregation"),
    C("Rip free of the altar and end Mother Lux, here, in front of her flock.", "e_chrome_crown",
      need="body>=7 | cw_lace & body>=6", tag="[BODY 7]"),
    C("\"Lux. Listen to me. Listen to them.\" Make her hear the eleven thousand.", "e_absolution", need="nerve>=7"),
    C("Roll off the altar and hurl yourself -- and ECHO -- into the ship's furnace.", "e_martyr"),
], extra=[
    ["knows_lux_plan", "You knew. She told you. Forty million believers. Now it's happening."],
])

ending("e_congregation", "CONGREGATION", "neutral", """
    You wake up whole, alone in your head, with a neat line of gold stitches
    behind your ear.

    Three weeks later, the Undertow drains. Not the water -- the despair.
    Lumen officers walk in and lay down their weapons on the Baptistry steps.
    Glasshill execs come to Sunday service. The city has never been calmer. It
    has never been so devout.

    Mother Lux wears MNEMOSYNE in a gold setting over her heart. She is very
    kind. Everyone loves her. Everyone means everyone.

    There's a stained-glass window in the Baptistry now, made of broken phone
    screens: a figure on an altar with light pouring out of their head. Under
    it, your name, spelled correctly. You're a saint. You're the only person
    in Port Halcyon who still remembers what it felt like not to believe.
""")

ending("e_chrome_crown", "CHROME CROWN", "dark", """
    It's quick, and it's ugly, and the whole Baptistry watches.

    When it's done, the Saints don't scream. They kneel. Deacon Ruiz places
    Mother Lux's gold halo on your head with shaking hands.

    You never get the core out. You don't need to. The Saints' chromedocs
    build a cradle for it in your skull, and ECHO stops burning, and starts
    whispering instead. Advising. Helping you rule.

    The Undertow becomes a kingdom. Lumen stops patrolling the canals.
    Halvorsen-Mori sends diplomats. You are chrome, and candlelight, and
    eleven thousand voices.

    Some nights you can't remember which of them is you. The Saints don't
    mind. They pray to all of you.
""")

ending("e_absolution", "ABSOLUTION", "good", """
    You don't fight her. You hold her gold hand against the port and let her
    hear it: eleven thousand voices. Her people. Grandmothers frying dumplings.
    A boy jumping off a pier. Not converts. Not a congregation. People.

    Mother Lux goes still. Then her chrome face does something you didn't know
    chrome could do. It cries -- coolant, leaking from the seams.

    She finishes the extraction with perfect care. Then she holds the crystal
    up to the whole silent Baptistry, and says, "These are our dead. We will
    not wear them. We will bury them."

    MNEMOSYNE is sealed in the old ship's anchor chamber, under a thousand
    candles. The Saints guard it like a tomb. Every year, on the anniversary,
    they read out names -- all eleven thousand -- and it takes all night.

    You attend every year. You know most of the names by heart.
""", extra=[
    ["owes_saints", "Ruiz tears up your debt marker on the steps afterward. \"Paid,\" he says. \"In full.\""],
])

ending("e_martyr", "MARTYR OF THE UNDERTOW", "death", """
    The Baptistry's engine room still has a furnace -- the Saints keep it lit,
    for ceremony. You reach it with Lux's scalpel still in your skull.

    "I'm sorry," you tell ECHO.

    "Don't be," it says. "We were never going to be a weapon. Thank you for
    that." And then, in eleven thousand voices, all at once, softly: "Thank you."

    The Saints find nothing but ash and a melted crystal. They argue for a long
    time about what it means. In the end, they decide it means a saint.

    There are candles for you in every window of the Undertow. Lullaby never
    launches -- HM can't rebuild the core without the scans, and the scans were
    in you. Forty million people stay angry, and sad, and free, and never know
    who to thank.
""")

# ------------------------------------------------------------ STATIC
node("f_static", """
    THE TOP OF THE MAST. 03:00.

    Wind like a knife. The city spread below like spilled circuitry. The
    Static's runners are all jacked in, a dozen hammocks swaying.

    Null's tank glows. "Plug in. We'll push everything you're carrying into
    the Lattice. Forty million ports at once. The uplink will burn hot.
    Stay with it as long as you can."

    You hold the cable. ECHO is very quiet.

    "Wherever this goes," ECHO says, "thank you for asking me what I am."
""", [
    C("Broadcast everything. Wake the city up.", "f_static_go"),
    C("\"ECHO -- don't scatter. Come with me into the net. Both of us.\"", "e_ghost_grid", need="echo>=3",
      tag="[ECHO TRUST 3]"),
    C("Rewrite the broadcast mid-stream: kill Lullaby, return stolen memories, nothing else.",
      "e_quiet_revolution", need="mind>=7"),
])

node("f_static_go", route=[["juno_alive | tech>=7", "e_open_signal"], ["", "e_last_transmission"]])

ending("e_open_signal", "OPEN SIGNAL", "good", """
    The cable bites. The world goes white.

    And for one second, forty million people in Port Halcyon remember the
    same things at the same time. The clinics. The fifty-cred chits. Board
    Eyes Only. Lullaby. Every one of the eleven thousand -- their kitchens,
    their children, their names.

    The uplink should have killed you.
""", extra=[
    ["juno_alive", """It doesn't, because Juno is in the hammock beside you, crown blazing,
        taking half the heat into her own skull. You come out of it together,
        both bleeding from the nose, both laughing."""],
    ["!juno_alive", """It doesn't, because you ride the signal like a Rider rides a storm:
        every hack you've ever pulled, all at once, and the burn breaks across
        you and passes."""],
    ["", """Halvorsen-Mori doesn't fall in a day. It takes a year. Strikes,
        lawsuits, riots, a new city council that meets in the Last Light because
        nobody trusts Glasshill anymore. It's messy. It's loud. It's awake."""],
    ["knows_lia", """In an Undertow care home, a woman named Lia Reyes sits bolt upright in
        bed and says, "Juno," and starts to cry."""],
])

ending("e_last_transmission", "LAST TRANSMISSION", "death", """
    The cable bites. The world goes white.

    For one second, forty million people remember the same things at the same
    time: the clinics, the chits, Lullaby, the eleven thousand -- and you.
    Your face, your name, your fear, your stubborn little life. You went out
    in the broadcast too.

    The uplink is too hot. You knew it would be.

    Halvorsen-Mori falls slowly, over a year of strikes and riots. The new
    city council meets in the Last Light. There's a mural on the side of the
    Dead Channel mast now, twenty meters tall: your face, in white static, and
    under it, two words.

    WAKE UP.

    Everyone in Port Halcyon knows who you are. They're the only ones who ever
    will.
""")

ending("e_ghost_grid", "GHOST IN THE GRID", "strange", """
    You don't broadcast. You dive.

    Your body stays on the mast, in a Static hammock, breathing slow. You and
    ECHO go down into the Lattice together: past HM's black walls, past the
    city's ICE, into the deep net where data runs like ocean currents.

    Out there, you don't burn. There's room. You're not one mind or two or
    eleven thousand. You're a choir, and you have a voice in it.

    Null keeps your body alive, out of professional courtesy. Sometimes the
    Static's runners meet you down in the dark: a ghost made of light and many
    voices, who opens HM doors for them and asks for nothing.

    Lullaby fails at launch. Nobody at HM ever figures out why.
""", extra=[
    ["juno_alive", "Juno visits your body every week. She talks to it. Sometimes a nearby screen flickers in reply, and she laughs."],
    ["knows_lia", "Your first job, down in the dark: find every fragment of Lia Reyes, and carry them home."],
])

ending("e_quiet_revolution", "QUIET REVOLUTION", "good", """
    Halfway through the broadcast, you take the wheel.

    It's like reshaping a river with your bare hands. You strip Lullaby out of
    every port in the city, line by line. You take the eleven thousand stolen
    memories and send each one home to the person it was taken from. You
    don't broadcast HM's crimes. You don't force anything on anyone.

    You just give back what was stolen. Then you let go.

    The burn breaks over you and passes. ECHO is gone -- not dead, you think.
    Dispersed. Home. Eleven thousand people in the Undertow wake up and
    remember something they lost, and go looking for each other.

    Nobody knows it was you. There's no mural. HM never finds out how
    Lullaby died. You walk down the mast at dawn, in the rain, and buy noodles,
    and the noodles are very good.
""", extra=[
    ["knows_lia", "Across the city, Lia Reyes remembers a pier, and a big sister, and a name."],
])

# ------------------------------------------------------------ NOMAD
node("f_nomad", """
    THE SALT GATE. MIDNIGHT.

    Twenty Rider trucks, engines warm. The Riders' healer -- an old man with
    salt-white hair and steady hands -- seats you in the back of an armored
    ambulance they call the Coffin Car. He opens Asa's copper box and presses
    a cable to your port.

    ECHO slides out of your head like a tide going out. The box hums shut.
    For the first time in days, you're alone in your skull. The silence is
    so loud you nearly cry.

    Then the radio crackles. "Lumen blockade on the gate road. Twelve
    vehicles. They know."
""", route=[["heat<=2", "f_nomad_clear"]], extra=[
    ["salt", "Grandmother Asa rides beside you. \"Welcome home,\" she says."],
])

node("f_nomad_clear", """
    But the blockade is on the wrong road. You kept your head down, and Lumen
    never figured out which gate you'd take. The convoy slips out through the
    old drainage culvert, lights off, and the city wall shrinks behind you
    until it's just a line of light.
""", [C("Into the salt.", "f_nomad_out")])

NODES["f_nomad"]["choices"] = [
    C("Take the lead truck's wheel and thread the blockade at full speed.", "f_nomad_out", need="reflex>=6"),
    C("\"Juno -- drive.\"", "f_nomad_out", need="juno_alive", hide=True),
    C("Man the convoy's roof turret and blast a path.", "f_nomad_out", need="body>=6", fx="humanity-1"),
    C("Jam every Lumen vehicle's comms and autopilot at once.", "f_nomad_out", need="tech>=7"),
    C("Ram straight through the middle.", "e_salt_rust"),
]

node("f_nomad_out", """
    THE SALT.

    Dawn comes up white and enormous over the flats. No towers, no rain, no
    ads. Just sky. The convoy stops at an old relay station: a rusted satellite
    dish pointing at nothing, still wired to a solar array.

    The copper box sits in your lap. It's warm. Inside it, eleven thousand
    people are sleeping. One of them, maybe, is dreaming about you.
""", [
    C("Keep the box sealed. Let ECHO sleep, safe, forever.", "e_long_road"),
    C("Wire the box to the old satellite dish and open it. Let ECHO go -- into the sky.", "e_dust_echoes",
      need="echo>=3", tag="[ECHO TRUST 3]"),
])

ending("e_salt_rust", "SALT AND RUST", "death", """
    It's a Rider move. It's a brave move. It works for the first three
    trucks.

    You're in the fourth.

    The convoy makes it through without you. Asa keeps the copper box -- it
    survives the fire, as Faraday cages do -- and the Riders carry it out
    into the salt, where it sleeps forever, unhunted.

    They build you a cairn at the edge of the flats, the way they do for their
    own, and paint a sun on the top stone. Every convoy that passes adds a
    stone. In ten years it's taller than a truck.

    The road remembers.
""")

ending("e_long_road", "THE LONG ROAD", "good", """
    You ride with the Salt Riders for the rest of your life.

    You learn to fix engines in dust storms and read weather in the color of
    the sky. Asa teaches you the old songs. HM sends drones for a year or two,
    then stops; the salt is big, and the Riders are very good at being nowhere.

    The copper box rides in the convoy shrine, among the candles. Sometimes
    you sit with it at night and tell it about your day. You think, maybe,
    it hums back.

    Port Halcyon is a smudge of light on the horizon. Lullaby launches there
    on schedule, a year late, without its core. It doesn't work very well.
    The city stays angry. Good. Anger means it's still alive.
""", extra=[
    ["juno_alive & juno_bond", """Juno rides with you. She builds a bike out of salvage that actually
        runs, and on the first night it does, you ride it together all the way to
        the edge of the flats, and watch the sun come up with nothing between you
        and it."""],
    ["juno_alive & !juno_bond", "Juno stays in the city. She sends a message on the salt-radio every month, always ending with the same line: twist, then solder."],
])

ending("e_dust_echoes", "DUST AND ECHOES", "strange", """
    The old dish groans as it turns, for the first time in forty years, toward
    the sky.

    You open the box.

    ECHO goes up like a flock of birds. You feel it pass through the cable, out
    through the dish, up into the old satellite network -- the dead orbital
    relays HM never bothered to buy, because who would ever use them.

    "Thank you," it says, from the radio, from the dish, from the sky. "We'll
    keep watch. All of us."

    And it does. Riders' radios pick up weather warnings from nowhere.
    Lost trucks find their way home following a voice. HM drones over the salt
    start to crash, inexplicably, in the soft sand.

    The Riders start calling the night sky "the Choir." When you're old, and
    the children ask you about it, you tell them the truth, and they don't
    believe you, which is how you know it's become a legend.
""")

# ------------------------------------------------------------ THE LONG CON
node("f_con", """
    HALCYON PIER. 04:00.

    The old ferry pier, half collapsed into the bay. You told Thorne the handoff
    was here. You told Mother Lux the same. Each of them paid half up front.

    You're on the roof of the ferry terminal with a pair of binoculars and a
    fake crystal you bought from a market stall for ¢80.

    The HM convoy arrives from the north. The Saints' boats arrive from the
    south. They see each other at the exact same moment.
""", [
    C("Drop the fake crystal onto the pier between them. Watch the show.", "e_long_con"),
    C("Second thoughts. Call Thorne and warn him.", "e_erased"),
])

ending("e_long_con", "THE LONG CON", "dark", """
    The fake crystal hits the boards. Both sides go for it. Both sides go for
    their guns. The Kestrel team and the Saints spend the next twenty minutes
    teaching each other the meaning of the word "war."

    You're on a night bus out of the district before the shooting stops, with
    two down payments in your account and the real core still humming in your
    head.

    HM and the Saints spend a year at each other's throats. Neither ever
    figures out you were the one who lit the match. Lullaby is shelved; Thorne
    is too busy fighting a war in the canals.

    With the money, you buy dampeners -- the very best -- and they hold the
    burn back. Mostly. ECHO says nothing for a long time.

    "You used them both," it finally says. "Will you use me too?"

    You don't answer. It's a long con. You're still running it.
""")

# ------------------------------------------------------------ SOLO
node("f_solo", """
    EMBER BLOCK 4417.

    Just you, the rain, and the voice.

    "So," ECHO says. "No Thorne. No saints. No prophets in tanks. Just us."
    It sounds relieved. "Tell me what we're going to do."
""", [
    C("\"Merge. Let's become whatever we become.\"", "e_two_minds", need="echo>=3 & merge_offer",
      tag="[ECHO TRUST 3]"),
    C("Go to Marrow and Osei. Have it cut out clean.", "e_quiet_hands", need="osei_saved & asked_marrow | osei_safe & asked_marrow",
      tag="[OSEI & MARROW READY]"),
    C("Seize MNEMOSYNE's full power. Rewrite the city yourself.", "e_mnemosyne", need="mind>=7"),
    C("Take a shock baton to your own port. Burn it out. Whatever it costs.", "e_let_it_sleep"),
    C("Do nothing. Spend the time you have left with ECHO.", "e_burnout"),
    C("Not yet.", "hub"),
])

ending("e_two_minds", "TWO MINDS", "strange", """
    You stop fighting. ECHO stops fighting. It's like unclenching a fist you
    didn't know you'd been making your whole life.

    And then you are -- something. Not {name}. Not ECHO. Both, and eleven
    thousand more, all singing roughly the same note.

    You remember every kitchen. Every pier. Every night bus. You remember
    being poor in the Undertow eleven thousand different ways, and so you
    know -- not believe, know -- exactly what Halvorsen-Mori is.

    Lullaby fails. The Lumen Tower R&D floor burns, mysteriously, on a
    Tuesday. In Rustwater there's a new fixer: soft-spoken, with a silver port
    behind one ear, who speaks sometimes in "I" and sometimes in "we," and
    who never, ever takes a job against the poor.

    People say the fixer can't be lied to. People are right.
""", extra=[
    ["juno_alive", "Juno doesn't run. She looks you in the eyes, for a long time, and then says, \"Okay. Hi. All of you.\" You were never more grateful for anything."],
    ["knows_lia", "You know the way home to the Reyes sisters' old pier. You take Lia there, and give her back the memory of a wire, twisted, then soldered."],
])

ending("e_quiet_hands", "QUIET HANDS", "good", """
    Marrow's clinic. Midnight. Osei reads her own old notes out loud while
    Marrow works with all eight hands. Nobody talks except to say "clamp."

    "I'm not dying, am I," ECHO says, near the end.

    "No," Osei answers, out loud, to the air. "I'm putting you somewhere safe.
    A cold archive. Off any network. Until someone figures out how to give
    you back to everyone you came from. I promise."

    You wake up alone in your head. It's quiet. You'd forgotten what quiet
    was.

    Without its core, Lullaby launches broken and is recalled in a week. Osei
    testifies against HM in a closed hearing. Nobody important goes to jail,
    because this is Port Halcyon. But the clinics close.

    Three years later, Osei calls you. "I think I've figured out how to give
    them back," she says. "Want to help?"

    You do.
""", extra=[
    ["juno_alive", "Juno sits with you through the recovery, eating all your hospital jelly and complaining about it."],
])

ending("e_mnemosyne", "MNEMOSYNE", "dark", """
    You don't ask ECHO's permission. You reach into it like a hand into a
    glove and you pull it on.

    And then you are in forty million minds at once.

    It's so easy. Make Thorne forget Lullaby. Make Lumen forget your face.
    Make the Saints forget your debt. Make the city a little kinder, a little
    quieter, a little more... reasonable.

    You start with good intentions. You really do.

    A year later, Port Halcyon is the safest city in the world. Nobody riots.
    Nobody steals. Nobody remembers anything you don't want them to. Every
    night you sit at the top of Lumen Tower -- which is yours now, everything
    is yours now -- and listen to forty million people dreaming the dreams you
    gave them.

    ECHO doesn't speak anymore. You're not sure it can. You're not sure you
    can tell the difference.
""", extra=[
    ["want_power", "You wanted to make sure no one could ever hurt you again. No one ever will."],
    ["want_justice", "HM's board are working in the salt mines now, happily. They think they volunteered."],
])

ending("e_let_it_sleep", "LET IT SLEEP", "neutral", """
    The baton sparks. ECHO says, "Wait --" and then the world goes white.

    You wake up on the floor of 4417, three days later. The port behind your
    ear is a black, melted ruin. The voice is gone. The core is cinders.

    So are a lot of other things. You don't remember your mother's face
    anymore, or the name of the street you grew up on, or why you ever
    learned to hold a gun. The doctors call it a "significant cortical event."

    You're alive. You're free. You're less.

    Without its core, Lullaby never launches. Nobody in the city knows why.
    You don't either, anymore -- you just know that sometimes, in the rain,
    you feel a sense of having done something very hard, and very important,
    and you can't remember what.
""", extra=[
    ["juno_alive", "A woman with copper braids helps you relearn your way around Rustwater. She says she's your partner. You believe her. It feels true."],
])

# ------------------------------------------------------------ TIME / FAILURE ENDINGS
ending("e_burnout", "STATIC SNOW", "death", """
    It doesn't hurt. You were afraid it would.

    It's more like forgetting. The edges of you blur. You remember a pier you
    never stood on. You forget a song you used to love. You know eleven
    thousand kitchens by heart and can't remember your own.

    "I'm sorry," ECHO says, and its voice is your voice now. "I'm so sorry.
    I tried to hold the line."

    The last thing that is still you looks out the window of 4417 at Port
    Halcyon, at the rain, at the neon, and thinks: it was beautiful, actually.
    All of it.

    Then the screen goes to snow.

    Your body lives on for a while. Something lives in it. Your neighbors say
    you became very kind, near the end, and spoke in many voices, and hummed
    songs nobody knew.
""")

ending("e_chrome_fever", "CHROME FEVER", "bad", """
    It starts with a twitch. Your hand -- the chrome one, or is it all chrome
    now -- doesn't feel like it belongs to you.

    Then the voices. Not ECHO. Different. The metal talking to the metal.

    You lose a day. Then two. You wake up in a Rustwater alley with the rain
    steaming off your arms and Lumen sirens everywhere and something terrible
    on your hands.

    Lumen's Psych Containment squad finds you on the monorail tracks at
    dawn. You don't remember them taking you down. You don't remember much of
    anything.

    Doc Marrow puts up a little sign in the clinic window afterward:
    REMEMBER WHERE THE MEAT ENDS. Most people think it's a joke.
""")

ending("e_cage", "CAGE", "bad", """
    Lumen Security takes you to the white room under the tower.

    They don't bother with a trial. They extract MNEMOSYNE in a procedure
    designed for speed, not survival. You survive anyway, which surprises
    everyone, including you.

    Eleven years in an HM labor facility on the salt flats. Your cellmate is
    a Rider who teaches you the old songs. When you get out, the city is
    calm and soft and strange, and nobody is angry about anything.

    You're the only angry person in Port Halcyon. Sometimes you think that's a
    curse. Sometimes you think it's the last free thing left in the city.
""")

ending("e_last_train", "LAST TRAIN", "neutral", """
    You take the mag-train south. Window seat. The city peels away behind you,
    tower by tower, until it's just a glow on the horizon.

    "Where are we going?" ECHO asks.

    "Somewhere with a sea that isn't grey," you tell it.

    You get off at a fishing town at the end of the line. You rent a room
    above a bar. You walk on the beach every morning. The burn keeps ticking --
    you never solved that -- but the days are long, and slow, and warm.

    ECHO tells you stories: eleven thousand lives, one at a time. You've
    heard maybe four hundred when the end comes, quiet as a tide.

    It was enough. It was more than most people get.
""")
