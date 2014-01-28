#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <sys/types.h>
#include <dirent.h>
#include <linux/limits.h>

#include <globalnetwork.h>
#include <hash.h>

int gni_secgroup_get_chainname(globalNetworkInfo *gni, gni_secgroup *secgroup, char **outchainname) {
  char hashtok[16 + 128 + 1];
  char chainname[48];
  char *chainhash=NULL;

  if (!gni || !secgroup || !outchainname) {
    LOGERROR("invalid input\n");
    return(1);
  }

  snprintf(hashtok, 16 + 128 + 1, "%s-%s", secgroup->accountId, secgroup->name);
  hash_b64enc_string(hashtok, &chainhash);
  if (chainhash) {
    snprintf(chainname, 48, "EU_%s", chainhash);
    *outchainname = strdup(chainname);
    EUCA_FREE(chainhash);
    return(0);
  }
  LOGERROR("could not create iptables compatible chain name for sec. group (%s)\n", secgroup->name);
  return(1);  
}

int gni_find_self_cluster(globalNetworkInfo *gni, gni_cluster **outclusterptr) {
  int rc, i, j;
  gni_node *outnodeptr=NULL;

  *outclusterptr = NULL;
  rc = gni_find_self_node(gni, &outnodeptr);
  if (outnodeptr) {
    for (i=0; i<gni->max_clusters; i++) {
      for (j=0; j<gni->clusters[i].max_nodes; j++) {
	if (!strcmp(gni->clusters[i].nodes[j].name, outnodeptr->name)) {
	  *outclusterptr = &(gni->clusters[i]);
	  return(0);
	}
      }
    }
  }
  return(1);
}

int gni_find_self_node(globalNetworkInfo *gni, gni_node **outnodeptr) {
  DIR *DH = NULL;
  struct dirent dent, *result = NULL;
  int max, rc, i, j, k;
  u32 *outips=NULL, *outnms=NULL;
  char *strptra;

  if (!gni || !outnodeptr) {
    LOGERROR("invalid input\n");
    return(1);
  }

  *outnodeptr = NULL;
  DH = opendir("/sys/class/net/");
  if (!DH) {
    LOGERROR("could not open directory /sys/class/net/ for read\n");
    return(1);
  }

  rc = readdir_r(DH, &dent, &result);
  while (!rc && result) {
    if (strcmp(dent.d_name, ".") && strcmp(dent.d_name, "..")) {
      rc = getdevinfo(dent.d_name, &outips, &outnms, &max);
      for (i=0; i<gni->max_clusters; i++) {
	for (j=0; j<gni->clusters[i].max_nodes; j++) {
	  for (k=0; k<max; k++) {
	    strptra = hex2dot(outips[k]);
	    if (strptra) {
	      if (!strcmp(strptra, gni->clusters[i].nodes[j].name)) {
		*outnodeptr = &(gni->clusters[i].nodes[j]);
		EUCA_FREE(strptra);
		EUCA_FREE(outips);
		EUCA_FREE(outnms);
		closedir(DH);
		return(0);
	      }
	      EUCA_FREE(strptra);
	    }
	  }
	}
      }
      EUCA_FREE(outips);
      EUCA_FREE(outnms);
    }
    rc = readdir_r(DH, &dent, &result);
  }
  closedir(DH);

  return(1);
}

int gni_cloud_get_clusters(globalNetworkInfo *gni, char **cluster_names, int max_cluster_names, char ***out_cluster_names, int *out_max_cluster_names, gni_cluster **out_clusters, int *out_max_clusters) {
  int ret=0, rc=0, getall=0, i=0, j=0, retcount=0, do_outnames=0, do_outstructs=0;
  gni_cluster *ret_clusters=NULL;
  char **ret_cluster_names=NULL;

  if (!cluster_names || max_cluster_names <= 0) {
    LOGERROR("invalid input\n");
    return(1);
  }

  if (out_cluster_names && out_max_cluster_names) {
    do_outnames=1;
  }
  if (out_clusters && out_max_clusters) {
    do_outstructs=1;
  }

  if (!do_outnames && !do_outstructs) {
    LOGDEBUG("nothing to do, both output variables are NULL\n");
    return(0);
  }

  if (do_outnames) {
    *out_cluster_names = NULL;
    *out_max_cluster_names = 0;
  }
  if (do_outstructs) {
    *out_clusters = NULL;
    *out_max_clusters = 0;
  }
  
  if (!strcmp(cluster_names[0], "*")) {
    getall = 1;
    if (do_outnames) *out_cluster_names = malloc(sizeof(char *) * gni->max_clusters);
    if (do_outstructs) *out_clusters = malloc(sizeof(gni_cluster) * gni->max_clusters);
  }

  if (do_outnames) ret_cluster_names = *out_cluster_names;
  if (do_outstructs) ret_clusters = *out_clusters;
  
  retcount=0;
  for (i=0; i<gni->max_clusters; i++) {
    if (getall) {
      if (do_outnames) ret_cluster_names[i] = strdup(gni->clusters[i].name);
      if (do_outstructs) memcpy(&(ret_clusters[i]), &(gni->clusters[i]), sizeof(gni_cluster));
      retcount++;
    } else {
      for (j=0; j<max_cluster_names; j++) {
	if (!strcmp(cluster_names[j], gni->clusters[i].name)) {
	  if (do_outnames) {
	    *out_cluster_names = realloc(*out_cluster_names, sizeof(char *) * (retcount+1));
	    ret_cluster_names = *out_cluster_names;
	    ret_cluster_names[retcount] = strdup(gni->clusters[i].name);
	  }
	  if (do_outstructs) {
	    *out_clusters = realloc(*out_clusters, sizeof(gni_cluster) * (retcount+1));
	    ret_clusters = *out_clusters;
	    memcpy(&(ret_clusters[retcount]), &(gni->clusters[i]), sizeof(gni_cluster)); 
	  }
	  retcount++;
	}
      }
    } 
  }
  if (do_outnames) *out_max_cluster_names = retcount;
  if (do_outstructs) *out_max_clusters = retcount;

  return(ret);
}
int gni_cluster_get_nodes(globalNetworkInfo *gni, gni_cluster *cluster, char **node_names, int max_node_names, char ***out_node_names, int *out_max_node_names, gni_node **out_nodes, int *out_max_nodes) {
  int ret=0, rc=0, getall=0, i=0, j=0, retcount=0, do_outnames=0, do_outstructs=0, out_max_clusters=0;
  gni_node *ret_nodes=NULL;
  gni_cluster *out_clusters=NULL;
  char **ret_node_names=NULL, **cluster_names=NULL;
  

  if (!node_names || max_node_names <= 0) {
    LOGERROR("invalid input\n");
    return(1);
  }

  if (out_node_names && out_max_node_names) {
    do_outnames=1;
  }
  if (out_nodes && out_max_nodes) {
    do_outstructs=1;
  }

  if (!do_outnames && !do_outstructs) {
    LOGDEBUG("nothing to do, both output variables are NULL\n");
    return(0);
  }

  if (do_outnames) {
    *out_node_names = NULL;
    *out_max_node_names = 0;
  }
  if (do_outstructs) {
    *out_nodes = NULL;
    *out_max_nodes = 0;
  }

  cluster_names = malloc(sizeof(char *) * 1);
  cluster_names[0] = cluster->name;
  rc = gni_cloud_get_clusters(gni, cluster_names, 1, NULL, NULL, &out_clusters, &out_max_clusters);
  if (rc || out_max_clusters <= 0) {
    LOGWARN("nothing to do, no matching cluster named '%s' found\n", cluster->name);
    return(0);
  }

  if (!strcmp(node_names[0], "*")) {
    getall = 1;
    if (do_outnames) *out_node_names = malloc(sizeof(char *) * out_clusters[0].max_nodes);
    if (do_outstructs) *out_nodes = malloc(sizeof(gni_node) * out_clusters[0].max_nodes);
  }

  if (do_outnames) ret_node_names = *out_node_names;
  if (do_outstructs) ret_nodes = *out_nodes;
  
  retcount=0;
  for (i=0; i<out_clusters[0].max_nodes; i++) {
    if (getall) {
      if (do_outnames) ret_node_names[i] = strdup(out_clusters[0].nodes[i].name);
      if (do_outstructs) memcpy(&(ret_nodes[i]), &(out_clusters[0].nodes[i]), sizeof(gni_node));
      retcount++;
    } else {
      for (j=0; j<max_node_names; j++) {
	if (!strcmp(node_names[j], out_clusters[0].nodes[i].name)) {
	  if (do_outnames) {
	    *out_node_names = realloc(*out_node_names, sizeof(char *) * (retcount+1));
	    ret_node_names = *out_node_names;
	    ret_node_names[retcount] = strdup(out_clusters[0].nodes[i].name);
	  }
	  if (do_outstructs) {
	    *out_nodes = realloc(*out_nodes, sizeof(gni_node) * (retcount+1));
	    ret_nodes = *out_nodes;
	    memcpy(&(ret_nodes[retcount]), &(out_clusters[0].nodes[i]), sizeof(gni_node)); 
	  }
	  retcount++;
	}
      }
    } 
  }
  if (do_outnames) *out_max_node_names = retcount;
  if (do_outstructs) *out_max_nodes = retcount;

  return(ret);

}
int gni_node_get_instances(globalNetworkInfo *gni, gni_node *node, char **instance_names, int max_instance_names, char ***out_instance_names, int *out_max_instance_names, gni_instance **out_instances, int *out_max_instances) {
  int ret=0, rc=0, getall=0, i=0, j=0, k=0, retcount=0, do_outnames=0, do_outstructs=0, out_max_nodes=0;
  gni_instance *ret_instances=NULL;
  gni_node *out_nodes=NULL;
  char **ret_instance_names=NULL, **node_names=NULL;
  

  if (!gni) {
    LOGERROR("invalid input\n");
    return(1);
  }

  if (out_instance_names && out_max_instance_names) {
    do_outnames=1;
  }
  if (out_instances && out_max_instances) {
    do_outstructs=1;
  }

  if (!do_outnames && !do_outstructs) {
    LOGDEBUG("nothing to do, both output variables are NULL\n");
    return(0);
  }

  if (do_outnames) {
    *out_instance_names = NULL;
    *out_max_instance_names = 0;
  }
  if (do_outstructs) {
    *out_instances = NULL;
    *out_max_instances = 0;
  }

  if (instance_names == NULL || !strcmp(instance_names[0], "*")) {
    getall = 1;
    if (do_outnames) *out_instance_names = malloc(sizeof(char *) * node->max_instance_names);
    if (do_outstructs) *out_instances = malloc(sizeof(gni_instance) * node->max_instance_names);
  }

  if (do_outnames) ret_instance_names = *out_instance_names;
  if (do_outstructs) ret_instances = *out_instances;
  
  retcount=0;
  for (i=0; i<node->max_instance_names; i++) {
    if (getall) {
      if (do_outnames) ret_instance_names[i] = strdup(node->instance_names[i].name);
      if (do_outstructs) {
	for (k=0; k<gni->max_instances; k++) {
	  if (!strcmp(gni->instances[k].name, node->instance_names[i].name)) {
	    memcpy(&(ret_instances[i]), &(gni->instances[k]), sizeof(gni_instance));
	    break;
	  }
	}
      }
      retcount++;
    } else {
      for (j=0; j<max_instance_names; j++) {
	if (!strcmp(instance_names[j], node->instance_names[i].name)) {
	  if (do_outnames) {
	    *out_instance_names = realloc(*out_instance_names, sizeof(char *) * (retcount+1));
	    ret_instance_names = *out_instance_names;
	    ret_instance_names[retcount] = strdup(node->instance_names[i].name);
	  }
	  if (do_outstructs) {
	    for (k=0; k<gni->max_instances; k++) {
	      if (!strcmp(gni->instances[k].name, node->instance_names[i].name)) {
		*out_instances = realloc(*out_instances, sizeof(gni_instance) * (retcount+1));
		ret_instances = *out_instances;
		memcpy(&(ret_instances[retcount]), &(gni->instances[k]), sizeof(gni_instance));
		break;
	      }
	    }
	  }
	  retcount++;
	}
      }
    } 
  }
  if (do_outnames) *out_max_instance_names = retcount;
  if (do_outstructs) *out_max_instances = retcount;

  return(ret);

}

int gni_instance_get_secgroups(globalNetworkInfo *gni, gni_instance *instance, char **secgroup_names, int max_secgroup_names, char ***out_secgroup_names, int *out_max_secgroup_names, gni_secgroup **out_secgroups, int *out_max_secgroups) {
  int ret=0, rc=0, getall=0, i=0, j=0, k=0, retcount=0, do_outnames=0, do_outstructs=0, out_max_instances=0;
  gni_secgroup *ret_secgroups=NULL;
  gni_instance *out_instances=NULL;
  char **ret_secgroup_names=NULL, **instance_names=NULL;
  
  if (!secgroup_names || max_secgroup_names <= 0) {
    LOGERROR("invalid input\n");
    return(1);
  }

  if (out_secgroup_names && out_max_secgroup_names) {
    do_outnames=1;
  }
  if (out_secgroups && out_max_secgroups) {
    do_outstructs=1;
  }

  if (!do_outnames && !do_outstructs) {
    LOGDEBUG("nothing to do, both output variables are NULL\n");
    return(0);
  }

  if (do_outnames) {
    *out_secgroup_names = NULL;
    *out_max_secgroup_names = 0;
  }
  if (do_outstructs) {
    *out_secgroups = NULL;
    *out_max_secgroups = 0;
  }

  if (!strcmp(secgroup_names[0], "*")) {
    getall = 1;
    if (do_outnames) *out_secgroup_names = malloc(sizeof(char *) * instance->max_secgroup_names);
    if (do_outstructs) *out_secgroups = malloc(sizeof(gni_secgroup) * instance->max_secgroup_names);
  }

  if (do_outnames) ret_secgroup_names = *out_secgroup_names;
  if (do_outstructs) ret_secgroups = *out_secgroups;
  
  retcount=0;
  for (i=0; i<instance->max_secgroup_names; i++) {
    if (getall) {
      if (do_outnames) ret_secgroup_names[i] = strdup(instance->secgroup_names[i].name);
      if (do_outstructs) {
	for (k=0; k<gni->max_secgroups; k++) {
	  if (!strcmp(gni->secgroups[k].name, instance->secgroup_names[i].name)) {
	    memcpy(&(ret_secgroups[i]), &(gni->secgroups[k]), sizeof(gni_secgroup));
	    break;
	  }
	}
      }
      retcount++;
    } else {
      for (j=0; j<max_secgroup_names; j++) {
	if (!strcmp(secgroup_names[j], instance->secgroup_names[i].name)) {
	  if (do_outnames) {
	    *out_secgroup_names = realloc(*out_secgroup_names, sizeof(char *) * (retcount+1));
	    ret_secgroup_names = *out_secgroup_names;
	    ret_secgroup_names[retcount] = strdup(instance->secgroup_names[i].name);
	  }
	  if (do_outstructs) {
	    for (k=0; k<gni->max_secgroups; k++) {
	      if (!strcmp(gni->secgroups[k].name, instance->secgroup_names[i].name)) {
		*out_secgroups = realloc(*out_secgroups, sizeof(gni_secgroup) * (retcount+1));
		ret_secgroups = *out_secgroups;
		memcpy(&(ret_secgroups[retcount]), &(gni->secgroups[k]), sizeof(gni_secgroup));
		break;
	      }
	    }
	  }
	  retcount++;
	}
      }
    } 
  }
  if (do_outnames) *out_max_secgroup_names = retcount;
  if (do_outstructs) *out_max_secgroups = retcount;

  return(ret);

}
int gni_secgroup_get_instances(globalNetworkInfo *gni, gni_secgroup *secgroup, char **instance_names, int max_instance_names, char ***out_instance_names, int *out_max_instance_names, gni_instance **out_instances, int *out_max_instances) {
  int ret=0, rc=0, getall=0, i=0, j=0, k=0, retcount=0, do_outnames=0, do_outstructs=0, out_max_secgroups=0;
  gni_instance *ret_instances=NULL;
  gni_secgroup *out_secgroups=NULL;
  char **ret_instance_names=NULL, **secgroup_names=NULL;
  
  if (!gni || !secgroup) {
    LOGERROR("invalid input\n");
    return(1);
  }

  if (out_instance_names && out_max_instance_names) {
    do_outnames=1;
  }
  if (out_instances && out_max_instances) {
    do_outstructs=1;
  }

  if (!do_outnames && !do_outstructs) {
    LOGDEBUG("nothing to do, both output variables are NULL\n");
    return(0);
  }

  if (do_outnames) {
    *out_instance_names = NULL;
    *out_max_instance_names = 0;
  }
  if (do_outstructs) {
    *out_instances = NULL;
    *out_max_instances = 0;
  }

  if ( (instance_names == NULL) || (!strcmp(instance_names[0], "*")) ) {
    getall = 1;
    if (do_outnames) *out_instance_names = malloc(sizeof(char *) * secgroup->max_instance_names);
    if (do_outstructs) *out_instances = malloc(sizeof(gni_instance) * secgroup->max_instance_names);
  }

  if (do_outnames) ret_instance_names = *out_instance_names;
  if (do_outstructs) ret_instances = *out_instances;
  
  retcount=0;
  for (i=0; i<secgroup->max_instance_names; i++) {
    if (getall) {
      if (do_outnames) ret_instance_names[i] = strdup(secgroup->instance_names[i].name);
      if (do_outstructs) {
	for (k=0; k<gni->max_instances; k++) {
	  if (!strcmp(gni->instances[k].name, secgroup->instance_names[i].name)) {
	    memcpy(&(ret_instances[i]), &(gni->instances[k]), sizeof(gni_instance));
	    break;
	  }
	}
      }
      retcount++;
    } else {
      for (j=0; j<max_instance_names; j++) {
	if (!strcmp(instance_names[j], secgroup->instance_names[i].name)) {
	  if (do_outnames) {
	    *out_instance_names = realloc(*out_instance_names, sizeof(char *) * (retcount+1));
	    ret_instance_names = *out_instance_names;
	    ret_instance_names[retcount] = strdup(secgroup->instance_names[i].name);
	  }
	  if (do_outstructs) {
	    for (k=0; k<gni->max_instances; k++) {
	      if (!strcmp(gni->instances[k].name, secgroup->instance_names[i].name)) {
		*out_instances = realloc(*out_instances, sizeof(gni_instance) * (retcount+1));
		ret_instances = *out_instances;
		memcpy(&(ret_instances[retcount]), &(gni->instances[k]), sizeof(gni_instance));
		break;
	      }
	    }
	  }
	  retcount++;
	}
      }
    } 
  }
  if (do_outnames) *out_max_instance_names = retcount;
  if (do_outstructs) *out_max_instances = retcount;

  return(ret);
}

int evaluate_xpath_property (xmlXPathContextPtr ctxptr, char *expression, char ***results, int *max_results) {
  int i;
  xmlXPathObjectPtr objptr;
  char **retresults;
  
  *max_results = 0;

  objptr = xmlXPathEvalExpression((unsigned char *)expression, ctxptr);
  if(objptr == NULL) {
    LOGERROR("unable to evaluate xpath expression: %s\n", expression);
    return(1);
  } else {
    if (objptr->nodesetval) {
      *max_results = (int)objptr->nodesetval->nodeNr;
      *results = malloc(sizeof(char *) * *max_results);
      retresults = *results;
      for (i=0; i<*max_results; i++) {
        retresults[i] = strdup((char *)objptr->nodesetval->nodeTab[i]->children->content);
      }
      LOGTRACE("%d results after evaluated expression %s\n", *max_results, expression);
      for (i=0; i<*max_results; i++) {
        LOGTRACE("\tRESULT %d: %s\n", i, retresults[i]);
      }
    }
  }
  xmlXPathFreeObject(objptr);
  return(0);
}

int evaluate_xpath_element (xmlXPathContextPtr ctxptr, char *expression, char ***results, int *max_results) {
  int i;
  xmlXPathObjectPtr objptr;
  char **retresults;

  *max_results = 0;

  objptr = xmlXPathEvalExpression((unsigned char *)expression, ctxptr);
  if(objptr == NULL) {
    LOGERROR("unable to evaluate xpath expression: %s\n", expression);
    return(1);
  } else {
    if (objptr->nodesetval) {
      *max_results = (int)objptr->nodesetval->nodeNr;
      *results = malloc(sizeof(char *) * *max_results);
      retresults = *results;
      for (i=0; i<*max_results; i++) {
        retresults[i] = strdup((char *)objptr->nodesetval->nodeTab[i]->properties->children->content);
      }
      LOGTRACE("%d results after evaluated expression %s\n", *max_results, expression);
      for (i=0; i<*max_results; i++) {
        LOGTRACE("\tRESULT %d: %s\n", i, retresults[i]);
      }
    }
  }
  xmlXPathFreeObject(objptr);
  return(0);
}

globalNetworkInfo *gni_init() {
  globalNetworkInfo *gni=NULL;
  gni = malloc(sizeof(globalNetworkInfo));
  if (!gni) {

  } else {
    bzero(gni, sizeof(globalNetworkInfo));
    gni->init = 1;
  }
  return(gni);
}

int gni_populate(globalNetworkInfo *gni, char *xmlpath) {
  int rc;
  xmlDocPtr docptr;
  xmlXPathContextPtr ctxptr;  
  char expression[2048], *strptra=NULL;
  char **results=NULL;
  int max_results=0, i, j, k, l, m;

  if (!gni) {
    LOGERROR("invalid input\n");
    return(1);
  }
  
  gni_clear(gni);

  xmlInitParser();
  LIBXML_TEST_VERSION
  
  docptr = xmlParseFile(xmlpath);
  if (docptr == NULL) {
    LOGERROR("unable to parse XML file (%s)\n", xmlpath);
    return(1);
  }
  
  ctxptr = xmlXPathNewContext(docptr);
  if(ctxptr == NULL) {
    LOGERROR("unable to get new xml context\n");
    xmlFreeDoc(docptr);
    return(1);
  }

  LOGDEBUG("begin parsing XML into data structures\n");

  // begin instance
  snprintf(expression, 2048, "/network-data/instances/instance");
  rc = evaluate_xpath_element(ctxptr, expression, &results, &max_results);
  gni->instances = malloc(sizeof(gni_instance) * max_results);
  for (i=0; i<max_results; i++) {
    LOGTRACE("after function: %d: %s\n", i, results[i]);
    snprintf(gni->instances[i].name, 16, "%s", results[i]);
    EUCA_FREE(results[i]);
  }
  gni->max_instances = max_results;
  EUCA_FREE(results);
  
  for (j=0; j<gni->max_instances; j++) {

    snprintf(expression, 2048, "/network-data/instances/instance[@name='%s']/ownerId", gni->instances[j].name);
    rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
    for (i=0; i<max_results; i++) {
      LOGTRACE("after function: %d: %s\n", i, results[i]);
      snprintf(gni->instances[j].accountId, 128, "%s", results[i]);
      EUCA_FREE(results[i]);
    }
    EUCA_FREE(results);

    snprintf(expression, 2048, "/network-data/instances/instance[@name='%s']/macAddress", gni->instances[j].name);
    rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
    for (i=0; i<max_results; i++) {
      LOGTRACE("after function: %d: %s\n", i, results[i]);
      mac2hex(results[i], gni->instances[j].macAddress);
      EUCA_FREE(results[i]);
    }
    EUCA_FREE(results);

    snprintf(expression, 2048, "/network-data/instances/instance[@name='%s']/publicIp", gni->instances[j].name);
    rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
    for (i=0; i<max_results; i++) {
      LOGTRACE("after function: %d: %s\n", i, results[i]);
      gni->instances[j].publicIp = dot2hex(results[i]);
      EUCA_FREE(results[i]);
    }
    EUCA_FREE(results);

    snprintf(expression, 2048, "/network-data/instances/instance[@name='%s']/privateIp", gni->instances[j].name);
    rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
    for (i=0; i<max_results; i++) {
      LOGTRACE("after function: %d: %s\n", i, results[i]);
      gni->instances[j].privateIp = dot2hex(results[i]);
      EUCA_FREE(results[i]);
    }
    EUCA_FREE(results);

    snprintf(expression, 2048, "/network-data/instances/instance[@name='%s']/securityGroups/value", gni->instances[j].name);
    rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
    gni->instances[j].secgroup_names = malloc(sizeof(gni_name) * max_results);
    for (i=0; i<max_results; i++) {
      LOGTRACE("after function: %d: %s\n", i, results[i]);
      snprintf(gni->instances[j].secgroup_names[i].name, 1024, "%s", results[i]);
      EUCA_FREE(results[i]);
    }
    gni->instances[j].max_secgroup_names = max_results;
    EUCA_FREE(results);
  }

  // end instance, begin secgroup
  snprintf(expression, 2048, "/network-data/securityGroups/securityGroup");
  rc = evaluate_xpath_element(ctxptr, expression, &results, &max_results);
  gni->secgroups = malloc(sizeof(gni_secgroup) * max_results);
  for (i=0; i<max_results; i++) {
    LOGTRACE("after function: %d: %s\n", i, results[i]);
    snprintf(gni->secgroups[i].name, 128, "%s", results[i]);
    EUCA_FREE(results[i]);
  }
  gni->max_secgroups = max_results;
  EUCA_FREE(results);
  
  
  for (j=0; j<gni->max_secgroups; j++) {

    // populate secgroup's instance_names
    gni->secgroups[j].max_instance_names = 0;
    gni->secgroups[j].instance_names = malloc(sizeof(gni_name) * gni->max_instances);
    for (k=0; k<gni->max_instances; k++) {
      for (l=0; l<gni->instances[k].max_secgroup_names; l++) {
	if (!strcmp(gni->instances[k].secgroup_names[l].name, gni->secgroups[j].name)) {
	  //	  gni->secgroups[j].instance_names = realloc(gni->secgroups[j].instance_names, sizeof(gni_name) * (gni->secgroups[j].max_instance_names + 1));
	  snprintf(gni->secgroups[j].instance_names[gni->secgroups[j].max_instance_names].name, 1024, "%s", gni->instances[k].name);
	  gni->secgroups[j].max_instance_names++;
	}
      }
    }

    snprintf(expression, 2048, "/network-data/securityGroups/securityGroup[@name='%s']/ownerId", gni->secgroups[j].name);
    rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
    for (i=0; i<max_results; i++) {
      LOGTRACE("after function: %d: %s\n", i, results[i]);
      snprintf(gni->secgroups[j].accountId, 128, "%s", results[i]);
      EUCA_FREE(results[i]);
    }
    EUCA_FREE(results);

    snprintf(expression, 2048, "/network-data/securityGroups/securityGroup[@name='%s']/rules/value", gni->secgroups[j].name);
    rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
    gni->secgroups[j].grouprules = malloc(sizeof(gni_name) * max_results);
    for (i=0; i<max_results; i++) {
      char newrule[2048];
      LOGTRACE("after function: %d: %s\n", i, results[i]);
      rc = ruleconvert(results[i], newrule);
      if (!rc) {
	snprintf(gni->secgroups[j].grouprules[i].name, 1024, "%s", newrule);
      }
      EUCA_FREE(results[i]);
    }
    gni->secgroups[j].max_grouprules = max_results;
    EUCA_FREE(results);
  }
  
  // end sec group, begin configuration

  snprintf(expression, 2048, "/network-data/configuration/property[@name='enabledCLCIp']/value");
  rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
  for (i=0; i<max_results; i++) {
    LOGTRACE("after function: %d: %s\n", i, results[i]);
    gni->enabledCLCIp = dot2hex(results[i]);
    EUCA_FREE(results[i]);
  }
  EUCA_FREE(results);

  snprintf(expression, 2048, "/network-data/configuration/property[@name='instanceDNSDomain']/value");
  rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
  for (i=0; i<max_results; i++) {
    LOGTRACE("after function: %d: %s\n", i, results[i]);
    snprintf(gni->instanceDNSDomain, HOSTNAME_SIZE, "%s", results[i]);
    EUCA_FREE(results[i]);
  }
  EUCA_FREE(results);

  snprintf(expression, 2048, "/network-data/configuration/property[@name='instanceDNSServers']/value");
  rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
  gni->instanceDNSServers = malloc(sizeof(u32) * max_results);
  for (i=0; i<max_results; i++) {
    LOGTRACE("after function: %d: %s\n", i, results[i]);
    gni->instanceDNSServers[i] = dot2hex(results[i]);
    EUCA_FREE(results[i]);
  }
  gni->max_instanceDNSServers = max_results;
  EUCA_FREE(results);

  snprintf(expression, 2048, "/network-data/configuration/property[@name='publicIps']/value");
  rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
  gni->public_ips = malloc(sizeof(u32) * max_results);
  for (i=0; i<max_results; i++) {
    LOGTRACE("after function: %d: %s\n", i, results[i]);
    gni->public_ips[i] = dot2hex(results[i]);
    EUCA_FREE(results[i]);
  }
  gni->max_public_ips = max_results;
  EUCA_FREE(results);
  
  snprintf(expression, 2048, "/network-data/configuration/property[@name='subnets']/subnet");
  rc = evaluate_xpath_element(ctxptr, expression, &results, &max_results);
  gni->subnets = malloc(sizeof(gni_subnet) * max_results);
  for (i=0; i<max_results; i++) {
    LOGTRACE("after function: %d: %s\n", i, results[i]);
    gni->subnets[i].subnet = dot2hex(results[i]);
    EUCA_FREE(results[i]);
  }
  gni->max_subnets = max_results;
  EUCA_FREE(results);
  
  for (j=0; j<gni->max_subnets; j++) {
    strptra = hex2dot(gni->subnets[j].subnet);
    snprintf(expression, 2048, "/network-data/configuration/property[@name='subnets']/subnet[@name='%s']/property[@name='netmask']/value", SP(strptra));
    EUCA_FREE(strptra);
    rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
    for (i=0; i<max_results; i++) {
      LOGTRACE("after function: %d: %s\n", i, results[i]);
      gni->subnets[j].netmask = dot2hex(results[i]);
      EUCA_FREE(results[i]);
    }
    EUCA_FREE(results);
    
    strptra = hex2dot(gni->subnets[j].subnet);
    snprintf(expression, 2048, "/network-data/configuration/property[@name='subnets']/subnet[@name='%s']/property[@name='gateway']/value", SP(strptra));
    EUCA_FREE(strptra);
    rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
    for (i=0; i<max_results; i++) {
      LOGTRACE("after function: %d: %s\n", i, results[i]);
      gni->subnets[j].gateway = dot2hex(results[i]);
      EUCA_FREE(results[i]);
    }
    EUCA_FREE(results);
  }
  
  snprintf(expression, 2048, "/network-data/configuration/property[@name='clusters']/cluster");
  rc = evaluate_xpath_element(ctxptr, expression, &results, &max_results);
  gni->clusters = malloc(sizeof(gni_cluster) * max_results);
  for (i=0; i<max_results; i++) {
    LOGTRACE("after function: %d: %s\n", i, results[i]);
    snprintf(gni->clusters[i].name, HOSTNAME_SIZE, "%s", results[i]);
    EUCA_FREE(results[i]);
  }
  gni->max_clusters = max_results;
  EUCA_FREE(results);
  
  for (j=0; j<gni->max_clusters; j++) {
    
    snprintf(expression, 2048, "/network-data/configuration/property[@name='clusters']/cluster[@name='%s']/property[@name='enabledCCIp']/value", gni->clusters[j].name);
    rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
    for (i=0; i<max_results; i++) {
      LOGTRACE("after function: %d: %s\n", i, results[i]);
      gni->clusters[j].enabledCCIp = dot2hex(results[i]);
      EUCA_FREE(results[i]);
    }
    EUCA_FREE(results);
    
    snprintf(expression, 2048, "/network-data/configuration/property[@name='clusters']/cluster[@name='%s']/property[@name='macPrefix']/value", gni->clusters[j].name);
    rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
    for (i=0; i<max_results; i++) {
      LOGTRACE("after function: %d: %s\n", i, results[i]);
      snprintf(gni->clusters[j].macPrefix, 8, "%s", results[i]);
      EUCA_FREE(results[i]);
    }
    EUCA_FREE(results);
    
    snprintf(expression, 2048, "/network-data/configuration/property[@name='clusters']/cluster[@name='%s']/property[@name='privateIps']/value", gni->clusters[j].name);
    rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
    gni->clusters[j].private_ips = malloc(sizeof(u32) * max_results);
    for (i=0; i<max_results; i++) {
      LOGTRACE("after function: %d: %s\n", i, results[i]);
      gni->clusters[j].private_ips[i] = dot2hex(results[i]);
      EUCA_FREE(results[i]);
    }
    gni->clusters[j].max_private_ips = max_results;
    EUCA_FREE(results);
    
    snprintf(expression, 2048, "/network-data/configuration/property[@name='clusters']/cluster[@name='%s']/subnet", gni->clusters[j].name);
    rc = evaluate_xpath_element(ctxptr, expression, &results, &max_results);
    for (i=0; i<max_results; i++) {
      LOGTRACE("after function: %d: %s\n", i, results[i]);
      gni->clusters[j].private_subnet.subnet = dot2hex(results[i]);
      EUCA_FREE(results[i]);
    }
    EUCA_FREE(results);
    
    strptra = hex2dot(gni->clusters[j].private_subnet.subnet);
    snprintf(expression, 2048, "/network-data/configuration/property[@name='clusters']/cluster[@name='%s']/subnet[@name='%s']/property[@name='netmask']/value", gni->clusters[j].name, SP(strptra));
    EUCA_FREE(strptra);
    rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
    for (i=0; i<max_results; i++) {
      LOGTRACE("after function: %d: %s\n", i, results[i]);
      gni->clusters[j].private_subnet.netmask = dot2hex(results[i]);
      EUCA_FREE(results[i]);
    }
    EUCA_FREE(results);
    
    strptra = hex2dot(gni->clusters[j].private_subnet.subnet);
    snprintf(expression, 2048, "/network-data/configuration/property[@name='clusters']/cluster[@name='%s']/subnet[@name='%s']/property[@name='gateway']/value", gni->clusters[j].name, SP(strptra));
    EUCA_FREE(strptra);
    rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
    for (i=0; i<max_results; i++) {
      LOGTRACE("after function: %d: %s\n", i, results[i]);
      gni->clusters[j].private_subnet.gateway = dot2hex(results[i]);
      EUCA_FREE(results[i]);
    }
    EUCA_FREE(results);
    
    snprintf(expression, 2048, "/network-data/configuration/property[@name='clusters']/cluster[@name='%s']/property[@name='nodes']/node", gni->clusters[j].name);
    rc = evaluate_xpath_element(ctxptr, expression, &results, &max_results);
    gni->clusters[j].nodes = malloc(sizeof(gni_node) * max_results);
    for (i=0; i<max_results; i++) {
      LOGTRACE("after function: %d: %s\n", i, results[i]);
      snprintf(gni->clusters[j].nodes[i].name, HOSTNAME_SIZE, "%s", results[i]);
      EUCA_FREE(results[i]);
    }
    gni->clusters[j].max_nodes = max_results;
    EUCA_FREE(results);
    
    for (k=0; k<gni->clusters[j].max_nodes; k++) {
      snprintf(expression, 2048, "/network-data/configuration/property[@name='clusters']/cluster[@name='%s']/property[@name='nodes']/node[@name='%s']/property[@name='dhcpdPath']/value", gni->clusters[j].name, gni->clusters[j].nodes[k].name);
      rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
      for (i=0; i<max_results; i++) {
	LOGTRACE("after function: %d: %s\n", i, results[i]);
	snprintf(gni->clusters[j].nodes[k].dhcpdPath, MAX_PATH, "%s", results[i]);
	EUCA_FREE(results[i]);
      }
      EUCA_FREE(results);
      
      snprintf(expression, 2048, "/network-data/configuration/property[@name='clusters']/cluster[@name='%s']/property[@name='nodes']/node[@name='%s']/property[@name='bridgeInterface']/value", gni->clusters[j].name, gni->clusters[j].nodes[k].name);
      rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
      for (i=0; i<max_results; i++) {
	LOGTRACE("after function: %d: %s\n", i, results[i]);
	snprintf(gni->clusters[j].nodes[k].bridgeInterface, 32, "%s", results[i]);
	EUCA_FREE(results[i]);
      }
      EUCA_FREE(results);
      
      snprintf(expression, 2048, "/network-data/configuration/property[@name='clusters']/cluster[@name='%s']/property[@name='nodes']/node[@name='%s']/property[@name='publicInterface']/value", gni->clusters[j].name, gni->clusters[j].nodes[k].name);
      rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
      for (i=0; i<max_results; i++) {
	LOGTRACE("after function: %d: %s\n", i, results[i]);
	snprintf(gni->clusters[j].nodes[k].publicInterface, 32, "%s", results[i]);
	EUCA_FREE(results[i]);
      }
      EUCA_FREE(results);
      
      snprintf(expression, 2048, "/network-data/configuration/property[@name='clusters']/cluster[@name='%s']/property[@name='nodes']/node[@name='%s']/instanceIds/value", gni->clusters[j].name, gni->clusters[j].nodes[k].name);
      rc = evaluate_xpath_property(ctxptr, expression, &results, &max_results);
      gni->clusters[j].nodes[k].instance_names = malloc(sizeof(gni_name) * max_results);
      for (i=0; i<max_results; i++) {
	LOGTRACE("after function: %d: %s\n", i, results[i]);
	snprintf(gni->clusters[j].nodes[k].instance_names[i].name, 1024, "%s", results[i]);
	EUCA_FREE(results[i]);
      }
      gni->clusters[j].nodes[k].max_instance_names = max_results;
      EUCA_FREE(results);
      
    }  
  }
  
  xmlXPathFreeContext(ctxptr);
  xmlFreeDoc(docptr);
  xmlCleanupParser();

  LOGDEBUG("end parsing XML into data structures\n");

  return(0);
}

int gni_iterate(globalNetworkInfo *gni, int mode) {
  int i, j, k;
  char *strptra = NULL;

  strptra = hex2dot(gni->enabledCLCIp);
  if (mode == GNI_ITERATE_PRINT) LOGTRACE("enabledCLCIp: %s\n", SP(strptra));
  EUCA_FREE(strptra);

  if (mode == GNI_ITERATE_PRINT) LOGTRACE("instanceDNSDomain: %s\n", gni->instanceDNSDomain);

  if (mode == GNI_ITERATE_PRINT) LOGTRACE("instanceDNSServers: \n");
  for (i=0; i<gni->max_instanceDNSServers; i++) {
    strptra = hex2dot(gni->instanceDNSServers[i]);
    if (mode == GNI_ITERATE_PRINT) LOGTRACE("\tdnsServer %d: %s\n", i, SP(strptra));
    EUCA_FREE(strptra);
  }
  if (mode == GNI_ITERATE_FREE) {
    EUCA_FREE(gni->instanceDNSServers);
  }

  if (mode == GNI_ITERATE_PRINT) LOGTRACE("publicIps: \n");
  for (i=0; i<gni->max_public_ips; i++) {
    strptra = hex2dot(gni->public_ips[i]);
    if (mode == GNI_ITERATE_PRINT) LOGTRACE("\tip %d: %s\n", i, SP(strptra));
    EUCA_FREE(strptra);
  }
  if (mode == GNI_ITERATE_FREE) {
    EUCA_FREE(gni->public_ips);
  }

  if (mode == GNI_ITERATE_PRINT) LOGTRACE("subnets: \n");
  for (i=0; i<gni->max_subnets; i++) {
    
    strptra = hex2dot(gni->subnets[i].subnet);
    if (mode == GNI_ITERATE_PRINT) LOGTRACE("\tsubnet %d: %s\n", i, SP(strptra));
    EUCA_FREE(strptra);
    
    strptra = hex2dot(gni->subnets[i].netmask);
    if (mode == GNI_ITERATE_PRINT) LOGTRACE("\t\tnetmask: %s\n", SP(strptra));
    EUCA_FREE(strptra);
    
    strptra = hex2dot(gni->subnets[i].gateway);
    if (mode == GNI_ITERATE_PRINT) LOGTRACE("\t\tgateway: %s\n", SP(strptra));
    EUCA_FREE(strptra);

  }
  if (mode == GNI_ITERATE_FREE) {   
    EUCA_FREE(gni->subnets);
  }

  if (mode == GNI_ITERATE_PRINT) LOGTRACE("clusters: \n");
  for (i=0; i<gni->max_clusters; i++) {
    if (mode == GNI_ITERATE_PRINT) LOGTRACE("\tcluster %d: %s\n", i, gni->clusters[i].name);
    strptra = hex2dot(gni->clusters[i].enabledCCIp);
    if (mode == GNI_ITERATE_PRINT) LOGTRACE("\t\tenabledCCIp: %s\n", SP(strptra));
    EUCA_FREE(strptra);

    if (mode == GNI_ITERATE_PRINT) LOGTRACE("\t\tmacPrefix: %s\n", gni->clusters[i].macPrefix);

    strptra = hex2dot(gni->clusters[i].private_subnet.subnet);
    if (mode == GNI_ITERATE_PRINT) LOGTRACE("\t\tsubnet: %s\n", SP(strptra));
    EUCA_FREE(strptra);

    strptra =  hex2dot(gni->clusters[i].private_subnet.netmask);
    if (mode == GNI_ITERATE_PRINT) LOGTRACE("\t\t\tnetmask: %s\n", SP(strptra));
    EUCA_FREE(strptra);

    strptra = hex2dot(gni->clusters[i].private_subnet.gateway);
    if (mode == GNI_ITERATE_PRINT) LOGTRACE("\t\t\tgateway: %s\n", SP(strptra));
    EUCA_FREE(strptra);

    if (mode == GNI_ITERATE_PRINT) LOGTRACE("\t\tprivate_ips \n");
    for (j=0; j<gni->clusters[i].max_private_ips; j++) {
      strptra = hex2dot(gni->clusters[i].private_ips[j]);
      if (mode == GNI_ITERATE_PRINT) LOGTRACE("\t\t\tip %d: %s\n", j, SP(strptra));
      EUCA_FREE(strptra);
    }
    if (mode == GNI_ITERATE_PRINT) LOGTRACE("\t\tnodes \n");
    for (j=0; j<gni->clusters[i].max_nodes; j++) {
      if (mode == GNI_ITERATE_PRINT) LOGTRACE("\t\t\tnode %d: %s\n", j, gni->clusters[i].nodes[j].name);
      if (mode == GNI_ITERATE_PRINT) LOGTRACE("\t\t\t\tdhcpdPath: %s\n", gni->clusters[i].nodes[j].dhcpdPath);
      if (mode == GNI_ITERATE_PRINT) LOGTRACE("\t\t\t\tbridgeInterface: %s\n", gni->clusters[i].nodes[j].bridgeInterface);
      if (mode == GNI_ITERATE_PRINT) LOGTRACE("\t\t\t\tpublicInterface: %s\n", gni->clusters[i].nodes[j].publicInterface);
      if (mode == GNI_ITERATE_FREE) {
	gni_node_clear(&(gni->clusters[i].nodes[j]));
      }
    }
    if  (mode == GNI_ITERATE_FREE) {
      EUCA_FREE(gni->clusters[i].nodes);
      gni_cluster_clear(&(gni->clusters[i]));
    }
  }
  if (mode == GNI_ITERATE_FREE) {
    EUCA_FREE(gni->clusters);
  }

  if (mode == GNI_ITERATE_PRINT) LOGTRACE("instances: \n");
  for (i=0; i<gni->max_instances; i++) {
    if (mode == GNI_ITERATE_PRINT) LOGTRACE("\tid: %s\n", gni->instances[i].name);
    if (mode == GNI_ITERATE_FREE) {
      gni_instance_clear(&(gni->instances[i]));
    }
  }
  if (mode == GNI_ITERATE_FREE) {
    EUCA_FREE(gni->instances);
  }

  if (mode == GNI_ITERATE_PRINT) LOGTRACE("secgroups: \n");
  for (i=0; i<gni->max_secgroups; i++) {
    if (mode == GNI_ITERATE_PRINT) LOGTRACE("\tname: %s\n", gni->secgroups[i].name);
    if (mode == GNI_ITERATE_FREE) {
      gni_secgroup_clear(&(gni->secgroups[i]));
    }
  }
  if (mode == GNI_ITERATE_FREE) {
    EUCA_FREE(gni->secgroups);
  }

  if (mode == GNI_ITERATE_FREE) {
    bzero(gni, sizeof(globalNetworkInfo));
    gni->init = 1;
  }

  return(0);
}


int gni_clear(globalNetworkInfo *gni) {
  return(gni_iterate(gni, GNI_ITERATE_FREE));
}

int gni_print(globalNetworkInfo *gni) {
  return(gni_iterate(gni, GNI_ITERATE_PRINT));
}

int gni_free(globalNetworkInfo *gni) {
  if (!gni) {
    return(0);
  }
  gni_clear(gni);
  EUCA_FREE(gni);
  return(0);
}

//!
//! Function description.
//!
//! @param[in] rulebuf
//! @param[in] outrule
//!
//! @return
//!
//! @see
//!
//! @pre List of pre-conditions
//!
//! @post List of post conditions
//!
//! @note
//!
int ruleconvert(char *rulebuf, char *outrule)
{
    int ret = 0;
    char proto[64], portrange[64], sourcecidr[64], icmptyperange[64], sourceowner[64], sourcegroup[64], newrule[4097], buf[2048];
    char *ptra = NULL, *toka = NULL, *idx = NULL;

    proto[0] = portrange[0] = sourcecidr[0] = icmptyperange[0] = newrule[0] = sourceowner[0] = sourcegroup[0] = '\0';

    if ((idx = strchr(rulebuf, '\n'))) {
        *idx = '\0';
    }

    toka = strtok_r(rulebuf, " ", &ptra);
    while (toka) {
        if (!strcmp(toka, "-P")) {
            toka = strtok_r(NULL, " ", &ptra);
            if (toka)
                snprintf(proto, 64, "%s", toka);
        } else if (!strcmp(toka, "-p")) {
            toka = strtok_r(NULL, " ", &ptra);
            if (toka)
                snprintf(portrange, 64, "%s", toka);
            if ((idx = strchr(portrange, '-'))) {
                char minport[64], maxport[64];
                sscanf(portrange, "%[0-9]-%[0-9]", minport, maxport);
                if (!strcmp(minport, maxport)) {
                    snprintf(portrange, 64, "%s", minport);
                } else {
                    *idx = ':';
                }
            }
        } else if (!strcmp(toka, "-s")) {
            toka = strtok_r(NULL, " ", &ptra);
            if (toka)
                snprintf(sourcecidr, 64, "%s", toka);
            if (!strcmp(sourcecidr, "0.0.0.0/0")) {
                sourcecidr[0] = '\0';
            }
        } else if (!strcmp(toka, "-t")) {
            toka = strtok_r(NULL, " ", &ptra);
            if (toka)
                snprintf(icmptyperange, 64, "any");
        } else if (!strcmp(toka, "-o")) {
            toka = strtok_r(NULL, " ", &ptra);
            if (toka)
                snprintf(sourcegroup, 64, toka);
        } else if (!strcmp(toka, "-u")) {
            toka = strtok_r(NULL, " ", &ptra);
            if (toka)
                snprintf(sourceowner, 64, toka);
        }
        toka = strtok_r(NULL, " ", &ptra);
    }

    LOGDEBUG("PROTO: %s PORTRANGE: %s SOURCECIDR: %s ICMPTYPERANGE: %s SOURCEOWNER: %s SOURCEGROUP: %s\n", proto, portrange, sourcecidr, icmptyperange, sourceowner, sourcegroup);

    // check if enough info is present to construct rule
    if (strlen(proto) && (strlen(portrange) || strlen(icmptyperange))) {
        if (strlen(sourcecidr)) {
            snprintf(buf, 2048, "-s %s ", sourcecidr);
            strncat(newrule, buf, 2048);
        }
        if (strlen(sourceowner) && strlen(sourcegroup)) {
            char ug[64], *chainhash = NULL;
            snprintf(ug, 64, "%s-%s", sourceowner, sourcegroup);
            hash_b64enc_string(ug, &chainhash);
            if (chainhash) {
                snprintf(buf, 2048, "-m set --set EU_%s src ", chainhash);
                strncat(newrule, buf, 2048);
                EUCA_FREE(chainhash);
            }
        }
        if (strlen(proto)) {
            snprintf(buf, 2048, "-p %s -m %s ", proto, proto);
            strncat(newrule, buf, 2048);
        }
        if (strlen(portrange)) {
            snprintf(buf, 2048, "--dport %s ", portrange);
            strncat(newrule, buf, 2048);
        }
        if (strlen(icmptyperange)) {
            snprintf(buf, 2048, "--icmp-type %s ", icmptyperange);
            strncat(newrule, buf, 2048);
        }

        while (newrule[strlen(newrule) - 1] == ' ') {
            newrule[strlen(newrule) - 1] = '\0';
        }

        snprintf(outrule, 2048, "%s", newrule);
        LOGDEBUG("CONVERTED RULE: %s\n", outrule);
    } else {
        LOGWARN("not enough information in RULE to construct iptables rule\n");
        ret = 1;
    }

    return (ret);
}

int gni_cluster_clear(gni_cluster *cluster) {
  int i;
  if (!cluster) {
    return(0);
  }

  EUCA_FREE(cluster->private_ips);

  bzero(cluster, sizeof(gni_cluster));
	
  return(0);
}

int gni_node_clear(gni_node *node) {
  int i;
  if (!node) {
    return(0);
  }

  EUCA_FREE(node->instance_names);

  bzero(node, sizeof(gni_node));

  return(0);  
}

int gni_instance_clear(gni_instance *instance) {
  int i;
  if (!instance) {
    return(0);
  }

  EUCA_FREE(instance->secgroup_names);
  
  bzero(instance, sizeof(gni_instance));
  
  return(0);
}

int gni_secgroup_clear(gni_secgroup *secgroup) {
  int i;
  if (!secgroup) {
    return(0);
  }

  EUCA_FREE(secgroup->grouprules);
  EUCA_FREE(secgroup->instance_names);
  
  bzero(secgroup, sizeof(gni_secgroup));
  
  return(0);
}
