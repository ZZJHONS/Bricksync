BrickSync
=========

**Inventory Synchronization Software between BrickLink and BrickOwl**

Contents:
---------

1.  [Overview](#overview)
2.  [Command List](#commandlist)

### Home Page

BrickSync features many commands to perform and assist inventory management.

_For new users expecting things to "just work", minimally learning usage of the [blmaster](#cmdblmaster) command is **important**._

This list of commands was last updated on **April 29th 2015** for the **version 1.5.6** of BrickSync.

<a id="overview"></a> Overview 
--------

General commands:

[status](#cmdstatus) [help](#cmdhelp) [check](#cmdcheck) [sync](#cmdsync) [verify](#cmdverify) [autocheck](#cmdautocheck) [about](#cmdabout) [message](#cmdmessage) [runfile](#cmdrunfile) [backup](#cmdbackup) [quit](#cmdquit) [prunebackups](#cmdprunebackups)

Inventory management commands:

[sort](#cmdsort) [blmaster](#cmdblmaster) [add](#cmdadd) [sub](#cmdsub) [loadprices](#cmdloadprices) [loadnotes](#cmdloadnotes) [loadmycost](#cmdloadmycost) [loadall](#cmdloadall) [merge](#cmdmerge) [invblxml](#cmdinvblxml) [invmycost](#cmdinvmycost)

Item management commands:

[find](#cmdfind) [item](#cmditem) [listempty](#cmdlistempty) [setquantity](#cmdsetquantity) [setprice](#cmdsetprice) [setcomments](#cmdsetcomments) [setremarks](#cmdsetremarks) [setblid](#cmdsetblid) [delete](#cmddelete) [owlresolve](#cmdowlresolve) [consolidate](#cmdconsolidate) [regradeused](#cmdregradeused)

Evaluation commands:

[evalset](#cmdevalset) [evalgear](#cmdevalgear) [evalpartout](#cmdevalpartout) [evalinv](#cmdevalinv) [checkprices](#cmdcheckprices)

Order commands:

[findorder](#cmdfindorder) [findordertime](#cmdfindordertime) [saveorderlist](#cmdsaveorderlist)

Catalog commands:

[owlqueryblid](#cmdowlqueryblid) [owlsubmitblid](#cmdowlsubmitblid) [owlupdateblid](#cmdowlupdateblid) [owlforceblid](#cmdowlforceblid) [owlsubmitdims](#cmdowlsubmitdims) [owlsubmitweight](#cmdowlsubmitweight)

<a id="commandlist"></a> Command List
------------

status  <a id="cmdstatus"><a/>

Print general information about BrickSync's current status.

help  <a id="cmdhelp"><a/>

Print the list of commands.

check <a id="cmdcheck"><a/>

Command syntax : check [bricklink|brickowl]  
The command flags both BrickLink and BrickOwl for an order check.  
Optionally, an argument bricklink or brickowl can be specified to only flag that specific service.  
If any new order or order change is found, the changes will be applied to the tracked inventory  
and to the other service.

sync  <a id="cmdsync"><a/>

Command syntax : sync [bricklink|brickowl]  
The command flags both BrickLink and BrickOwl for a deep synchronization.  
Optionally, an argument bricklink or brickowl can be specified to only flag that specific service.  
During deep synchronization, the service's inventory is compared against BrickSync's local inventory.  
All changes required to make the service's inventory match the local one are then applied.  
The command uses as few API calls as possible to make the inventories match.

verify <a id="cmdverify"><a/>

Command syntax : verify [bricklink|brickowl]  
The command verifies if BrickLink and BrickOwl match the local inventory.  
It lists all the operations that would be executed by a sync command, but does not  
apply the operations required to synchronize the remote inventory.  
Only a summary is printed in the terminal, consult the log file for detailed information.  
Optionally, an argument bricklink or brickowl can be specified to only verify that specific service.

autocheck  <a id="cmdautocheck"><a/>

Command syntax : autocheck on|off  
The command changes the value of the autocheck mode.  
When enabled, services are checked regularly for new orders, at intervals specified in the configuration file.

about  <a id="cmdabout"><a/>

Command syntax : about  
The command prints some general information about BrickSync.

message  <a id="cmdmessage"><a/>

Command syntax : message [update|discard]  
Print the latest BrickSync broadcast message if any.  
An argument of update will check for any new broadcast message.  
An argument of discard will discard an already received message.

runfile  <a id="cmdrunfile"><a/>

Command syntax : runfile pathtofile.txt  
Run all the commands found in the specified file.

backup  <a id="cmdbackup"><a/>

Command syntax : backup NewBackup.bsx  
The command saves the current inventory as a BSX file at the path specified.

quit  <a id="cmdquit"><a/>

Command syntax : quit  
The command quits BrickSync.  
Operations will resume safely when BrickSync is launched again.

prunebackups  <a id="cmdprunebackups"><a/>

Command syntax : prunebackups [-p] CountOfDays  
The command deletes BrickSync's automated backups of your inventory older than the specified count of days.  
Use the pretend flag (\-p) to see the disk space taken without deleting anything.

sort  <a id="cmdsort"><a/>

Command syntax : sort SomeBsxFile.bsx  
The command reads and updates a specified BrickStore/BrickStock BSX file.  
The fields comments, remarks and price are updated for all items found in the local inventory.  
Lots with no match in the local inventory are left unchanged.  
This command is meant to assist the physical sorting of parts in your inventory, usually before an add or merge command is used to merge the inventory.

blmaster <a id="cmdblmaster"><a/>

Command syntax : blmaster on|off  
The command enters or exit the BrickLink Master Mode, depending on the on or off argument.  
When you enter this mode, all synchronization and order checks are suspended, and you can edit your inventory directly on BrickLink as much as you like.  
When you exit this mode, the tracked inventory and BrickOwl both are synchronized with the changes applied to the BrickLink inventory.  
New BrickLink or BrickOwl orders received while the BrickLink Master Mode was active are properly integrated.  
Example: blmaster on, _Edit your inventory on BrickLink as desired_, blmaster off

add  <a id="cmdadd"><a/>

Command syntax : add BrickStoreFile.bsx  
The command adds a BSX file to the tracked inventory.  
Existing lots are merged, quantities are incremented.  
Comments, remarks, prices and other settings are left unchanged.

sub  <a id="cmdsub"><a/>

Command syntax : sub BrickStoreFile.bsx  
The command subtracts a BSX file from the tracked inventory.  
Quantities are decremented for all existing lots.  
Comments, remarks, prices and other settings are left unchanged.

loadprices  <a id="cmdloadprices"><a/>

Command syntax : loadprices BrickStoreFile.bsx  
The command updates the prices of the tracked inventory for all matching lots from the specified file.  
Lots are matched by LotID if available, otherwise by BLID:Color:Condition.  
Lots from the BSX file that aren't found in the tracked inventory are ignored.

loadnotes  <a id="cmdloadnotes"><a/>

Command syntax : loadnotes BrickStoreFile.bsx  
The command updates the comments and remarks of the tracked inventory for all matching lots from the specified file.  
Lots are matched by LotID if available, otherwise by BLID:Color:Condition.  
Lots from the BSX file that aren't found in the tracked inventory are ignored.

loadmycost  <a id="cmdloadmycost"><a/>

Command syntax : loadmycost BrickStoreFile.bsx  
The command updates the mycost values of the tracked inventory for all matching lots from the specified file.  
Lots are matched by LotID if available, otherwise by BLID:Color:Condition.  
Lots from the BSX file that aren't found in the tracked inventory are ignored.

loadall  <a id="cmdloadall"><a/>

Command syntax : loadall BrickStoreFile.bsx  
The command updates the comments, remarks, price, mycost and bulk of the tracked inventory for all matching lots from the specified file.  
Lots are matched by LotID if available, otherwise by BLID:Color:Condition.  
Lots from the BSX file that aren't found in the tracked inventory are ignored.

merge  <a id="cmdmerge"><a/>

Command syntax : merge BrickStoreFile.bsx  
The command merges a BSX file into the tracked inventory.  
New lots are created. Existing lots are merged, quantities are incremented, all fields are updated.  
This is the functional equivalent of the command add followed by loadall.

invblxml  <a id="cmdinvblxml"><a/>

Command syntax : invblxml SomeBsxFile.bsx  
The command reads a specified BrickStore/BrickStock BSX file and outputs BrickLink XML files for it.  
The XML will be saved as blupload000.xml.txt in the current working directory.  
The output may be broken in multiple XML files if a single file would exceed BrickLink's maximum size for XML upload.

invmycost  <a id="cmdinvmycost"><a/>

Command syntax : invmycost SomeBsxFile.bsx TotalMyCost  
The command reads and updates a specified BrickStore/BrickStock BSX file.  
The field MyCost of each lot is updated to sum up to TotalMyCost, proportionally to the Cost of each lot.

find  <a id="cmdfind"><a/>

Command syntax : find term0 [term1] [term2] ...  
The command searches the tracked inventory for lots that match all specified terms. All item fields are searched for matches.  
A single search term can include spaces if specified within quotes. The search is case sensitive.  
Example: find 3001 "uish Gray"  
Example: find 3794 "ish Brow"

item  <a id="cmditem"><a/>

Command syntax : item ItemIdentifier  
The command prints all available information for an item. There are multiple ways to specify the ItemIdentifier argument.  
The item identifier can be BLLotID, example: item 55985466  
The item identifier can be \*BOLotID, example: item \*2727357  
The item identifier can be BLID:BLColor:Condition, example: item 3001:11:N  
The item identifier can be BOID-BOColor-Condition, example: item 771344-38-N  
When applicable, the condition parameter (N or U) is optional, it is assumed N if ommited.  
If multiple items match (duplicate lots), only the first item found is displayed. See the find command to list all matches.

listempty  <a id="cmdlistempty"><a/>

Command syntax : listempty  
The command lists all lots with a quantity of zero.  
You probably don't have empty lots unless the configuration variable retainemptylots was enabled.

setquantity  <a id="cmdsetquantity"><a/>

Command syntax : setquantity ItemIdentifier (+/-)Number  
The command adjusts the quantity for the specified item. The quantity can be absolute, or relative by specifying a '+' or '-' sign in front of the quantity.  
See the command item for the syntax to identify an item.

setprice  <a id="cmdsetprice"><a/>

Command syntax : setprice ItemIdentifier NewPrice  
The command sets the price for the specified item.  
See the command item for the syntax to identify an item.

setcomments  <a id="cmdsetcomments"><a/>

Command syntax : setcomments ItemIdentifier "My New Comments"  
The command sets the comments for the specified item. Note that you'll want to use quotes to specify comments including spaces.  
See the command item for the syntax to identify an item.

setremarks  <a id="cmdsetremarks"><a/>

Command syntax : setremarks ItemIdentifier "My New Remarks"  
The command sets the remarks for the specified item. Note that you'll want to use quotes to specify remarks including spaces.  
See the command item for the syntax to identify an item.

setblid  <a id="cmdsetblid"><a/>

Command syntax : setblid ItemIdentifier NewBLID  
The command changes the BLID of the specified item.  
Note that the command will actually delete and recreate new lots on both BrickLink and BrickOwl.  
See the command item for the syntax to identify an item.

delete  <a id="cmddelete"><a/>

Command syntax : delete ItemIdentifier  
The command deletes the item entirely, even if the configuration variable retainemptylots was enabled.  
See the command item for the syntax to identify an item.

owlresolve  <a id="cmdowlresolve"><a/>

Command syntax : owlresolve  
The command tries to resolve the BOID of all items for which we presently don't have a BOID.  
It also prints all items currently unknown to BrickOwl, so that you can go add them to the database. :)  
If new items were resolved, typing sync brickowl will now upload the items to BrickOwl.

consolidate  <a id="cmdconsolidate"><a/>

Command syntax : consolidate [-f]  
The command merges all lots with identical BLID, color, condition, comments and remarks.  
Lots that only differ through their comments or remarks are printed out.  
The \-f flag forces consolidation even if comments or remarks differ.

regradeused  <a id="cmdregradeused"><a/>

Command syntax : regradeused  
The command updates the grade of all items in used condition.  
The comments of each lot are parsed to determine the appropriate quality grades.  
The parser recognizes many keywords and understands basic grammar rules from modifier keywords (such as 'not' or 'many') to form compound statements.  
Keep your comments simple and the parser should do a reasonable job at assigning proper grades for all used items.

evalset <a id="cmdevalset"><a/>

Command syntax : evalset SetNumber [EvalSetInv.bsx]  
The command evaluates a set for partout, comparing how it would combine and improve the current inventory.  
The optional third argument specifies an output BSX file where to write the set's inventory with per-part statistics.  
When alternates for a part exist, only the cheapest one is added to the BSX file.  
Price guide information is fetched without consuming API calls.

evalgear <a id="cmdevalgear"><a/>

Command syntax : evalgear GearNumber [EvalGearInv.bsx]  
The command evaluates a gear for partout, comparing how it would combine and improve the current inventory.  
The optional third argument specifies an output BSX file where to write the gear's inventory with per-part statistics.  
When alternates for a part exist, only the cheapest one is added to the BSX file.  
Price guide information is fetched without consuming API calls.

evalpartout <a id="cmdevalpartout"><a/>

Command syntax : evalpartout SetNumber [EvalSetInv.bsx]  
The command evaluates a set for partout, comparing how it would combine and improve the current inventory.  
The optional third argument specifies an output BSX file where to write the set's inventory with per-part statistics.  
All the alternates are added to the BSX file, although printed global statistics only consider the cheapest alternate.  
Price guide information is fetched without consuming API calls.

evalinv <a id="cmdevalinv"><a/>

Command syntax : evalinv InventoryToEvaluate.bsx [EvalInv.bsx]  
The command evaluates a BSX inventory, comparing how it would combine and improve the current inventory.  
The optional third argument specifies an output BSX file where to write the inventory with per-part statistics.  
Price guide information is fetched without consuming API calls.

checkprices <a id="cmdcheckprices"><a/>

Command syntax : checkprices [low] [high]  
The command checks the prices of the current inventory compared to price guide information.  
It lists any price that falls outside of the relative range defined by the low to high bounds.  
If ommited, the low and high parameters are defined as 0.5 and 1.5.

findorder <a id="cmdfindorder"><a/>

Command syntax : findorder term0 [term1] [term2] ...  
The command searches past orders for an item that matches all the terms specified.  
Groups of words can be searched by enclosing them in quotation marks.  
Use the command findordertime to change how far back to search, in days.

findordertime <a id="cmdfindordertime"><a/>

Command syntax : findordertime CountOfDays  
The command sets the count of days that subsequent invocations of findordertime will search.  
The search time considers the last time the order's inventory was updated on disk.

saveorderlist <a id="cmdsaveorderlist"><a/>

Command syntax : saveorderlist CountOfDays  
The command fetches the list of orders from both BrickLink and BrickOwl received in the time range specified, in days.  
The list is saved to disk as the files orderlist.txt and orderlist.tab.txt, as text and tab-delimited files respectively.  
The list and summary is also printed in the terminal. The command consumes only one BrickLink API call.

owlqueryblid <a id="cmdowlqueryblid"><a/>

Command syntax : owlqueryblid BLID  
Query the BrickOwl database to resolve a BOID for the specified BLID.

owlsubmitblid <a id="cmdowlsubmitblid"><a/>

Command syntax : owlsubmitblid BOID BLID  
The command edits the BrickOwl catalog, submit a new BLID for a given BOID.  
Note that it may take up to 48 hours for a catalog administrator to approve the submission.

owlupdateblid <a id="cmdowlupdateblid"><a/>

Command syntax : owlupdateblid oldBLID newBLID  
The command edits the BrickOwl catalog. It resolves the BOID for the specified oldBLID, then submit a newBLID to that resolved BOID.  
The command will fail if oldBLID can not be resolved to a BOID.  
Note that it may take up to 48 hours for a catalog administrator to approve the submission.

owlforceblid <a id="cmdowlforceblid"><a/>

Command syntax : owlforceblid BOID BLID  
The command appends a new entry in BrickSync's translation cache. It permanently maps a BLID to a BOID.  
The command does not replace updating the BrickOwl catalog database, manually or through owlsubmitblid.  
Yet, if you are waiting for the approval of BLID submissions, the command allows uploading these items to BrickOwl right away.

owlsubmitdims <a id="cmdowlsubmitdims"><a/>

Command syntax : owlsubmitdims ID length width height  
The command edits the BrickOwl catalog.  
Note that it may take up to 48 hours for a catalog administrator to approve the submission.

owlsubmitweight <a id="cmdowlsubmitweight"><a/>

Command syntax : owlsubmitweight ID weight  
The command edits the BrickOwl catalog.  
Note that it may take up to 48 hours for a catalog administrator to approve the submission.

Special thanks to BrickFeverParts, budgetkids, DadsAFOL, playonbricks, PSBricks and many others for the beta version testing and support!

LEGO, the LEGO logo are trademarks of The LEGO Group of companies.