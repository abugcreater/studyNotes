# MongoDB使用技巧

**查看文档大小**

`Object.bsonsize(db.tms_province_agg_result.findOne())`

**查看集合中平均文档大小**

`db.tms_province_agg_result.stats().avgObjSize;`

参考:[MongoDB 如何查看文档的大小](https://www.modb.pro/db/41182)



**根据数组大小查询**

先确定存在该字段,然后使用where语句查询

`db.domain.find( {tag : {$exists:true}, $where:'this.tag.length>3'} )`

**数组修改器**

使用 $push新增数组元素

`db.getCollection("CloudBeanAnswerSet").updateOne({"_id" : ObjectId("63db5365923eb80edcbfc25e")}, {$push:{"stuAnswers":aa.stuAnswers[0]}})`

参考:[spring mongodb数组修改器](https://blog.csdn.net/niclascage/article/details/47009989?spm=1001.2101.3001.6650.1&utm_medium=distribute.pc_relevant.none-task-blog-2%7Edefault%7ECTRLIST%7ERate-1-47009989-blog-78697113.pc_relevant_recovery_v2&depth_1-utm_source=distribute.pc_relevant.none-task-blog-2%7Edefault%7ECTRLIST%7ERate-1-47009989-blog-78697113.pc_relevant_recovery_v2&utm_relevant_index=2)



**查询库中所有索引**

```js
var collectionList = db.getCollectionNames();
for(var index in collectionList){
    var collection = collectionList[index];
        //兼容唯一索引情况,修改 2020年8月7日 16:50:32
		var cur = db.getCollection(collection).getIndexes();
		if(cur.length == 1){
			continue;
		}
		for(var index1 in cur){
			var next = cur[index1];
			 //兼容唯一索引情况,修改 2020年8月7日 16:50:32
			if(next["key"]["_id"] == '1'){
				continue;
			}
       print("try{ db.getCollection(\""+collection+"\").ensureIndex("+JSON.stringify(next.key)+",{background:1, unique:" + (next.unique || false) + " })}catch(e){print(e)}");
		}
	}
```

[导出mongo索引，获取mongo所有数据表中的索引](https://blog.csdn.net/badyting/article/details/84753790)